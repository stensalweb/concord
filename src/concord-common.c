#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <libconcord.h>
#include "concord-common.h"

#include "hashtable.h"
#include "debug.h"


void
Concord_http_request(
  concord_st *concord, 
  void **p_object, 
  concord_load_obj_ft *load_cb,
  enum http_method http_method,
  char endpoint[],
  ...)
{
  /* create url_route */
  va_list args;
  va_start (args, endpoint);

  char url_route[MAX_URL_LEN];
  vsprintf(url_route, endpoint, args);

  va_end(args);

  /* try to get major parameter for bucket key, if doesn't
      exists then will return the endpoint instead */
  char *bucket_key = Concord_tryget_major(endpoint);
  DEBUG_PRINT("Bucket key encountered: %s", bucket_key);

  Concord_bucket_build(
             concord->utils,
             p_object,
             load_cb,
             http_method,
             bucket_key,
             url_route);

  if (MAX_CONCURRENT_CONNS == concord->utils->transfers_onhold){
    DEBUG_PUTS("Reach max concurrent connections threshold, auto performing connections on hold ...");
    concord_dispatch(concord);
  }
}

static concord_utils_st*
_concord_utils_init(char token[])
{
  concord_utils_st *new_utils = safe_malloc(sizeof *new_utils);

  new_utils->loop = uv_default_loop();

  new_utils->token = strndup(token, strlen(token)-1);
  DEBUG_ASSERT(NULL != new_utils->token, "Out of memory");

  new_utils->request_header = Curl_request_header_init(new_utils);

  uv_timer_init(new_utils->loop, &new_utils->timeout);
  new_utils->timeout.data = new_utils;

  new_utils->multi_handle = Curl_multi_default_init(new_utils);

  new_utils->bucket_dict = dictionary_init();
  dictionary_build(new_utils->bucket_dict, BUCKET_DICTIONARY_SIZE);

  new_utils->header = dictionary_init();
  dictionary_build(new_utils->header, HEADER_DICTIONARY_SIZE);

  return new_utils;
}

static void
_uv_on_walk_cb(uv_handle_t *handle, void *arg)
{
  uv_close(handle, NULL);
}

static void
_concord_utils_destroy(concord_utils_st *utils)
{
  int uvcode = uv_loop_close(utils->loop);
  if (UV_EBUSY == uvcode){ //there are still handles that need to be closed
    uv_walk(utils->loop, &_uv_on_walk_cb, NULL); //close each handle encountered

    uvcode = uv_run(utils->loop, UV_RUN_DEFAULT); //run the loop again to close remaining handles
    DEBUG_ASSERT(!uvcode, uv_strerror(uvcode));

    uvcode = uv_loop_close(utils->loop); //finally, close the loop
    DEBUG_ASSERT(!uvcode, uv_strerror(uvcode));
  }
  
  curl_slist_free_all(utils->request_header);
  curl_multi_cleanup(utils->multi_handle);

  dictionary_destroy(utils->bucket_dict);
  dictionary_destroy(utils->header);

  safe_free(utils->client_buckets);

  safe_free(utils->token);

  safe_free(utils);
}

concord_st*
concord_init(char token[])
{
  concord_st *new_concord = safe_malloc(sizeof *new_concord);

  new_concord->utils = _concord_utils_init(token);

  new_concord->channel = concord_channel_init(&new_concord->utils);
  new_concord->guild = concord_guild_init(&new_concord->utils);
  new_concord->user = concord_user_init(&new_concord->utils);
  new_concord->client = concord_user_init(&new_concord->utils);

  return new_concord;
}

void
concord_cleanup(concord_st *concord)
{
  _concord_utils_destroy(concord->utils);

  concord_channel_destroy(concord->channel);
  concord_guild_destroy(concord->guild);
  concord_user_destroy(concord->user);
  concord_user_destroy(concord->client);

  safe_free(concord);
}

void
concord_global_init(){
  int code = curl_global_init(CURL_GLOBAL_DEFAULT);
  DEBUG_ASSERT(!code, "Couldn't start curl_global_init()");
}

void
concord_global_cleanup(){
  curl_global_cleanup();
}
