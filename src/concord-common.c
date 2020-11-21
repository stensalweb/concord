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
  concord_http_t *http, 
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

  Concord_bucket_build(
             http,
             p_object,
             load_cb,
             http_method,
             bucket_key,
             url_route);
}

static concord_http_t*
_concord_http_init(char token[])
{
  concord_http_t *new_http = safe_calloc(1, sizeof *new_http);

  new_http->loop = uv_default_loop();
  uv_loop_set_data(new_http->loop, new_http);

  new_http->token = strndup(token, strlen(token)-1);
  DEBUG_ASSERT(NULL != new_http->token, "Out of memory");

  new_http->request_header = Curl_request_header_init(new_http);

  uv_timer_init(new_http->loop, &new_http->timeout);
  uv_handle_set_data((uv_handle_t*)&new_http->timeout, new_http);

  new_http->multi_handle = Concord_http_multi_init(new_http);

  new_http->bucket_dict = dictionary_init();
  dictionary_build(new_http->bucket_dict, BUCKET_DICTIONARY_SIZE);

  new_http->header = dictionary_init();
  dictionary_build(new_http->header, HEADER_DICTIONARY_SIZE);

  return new_http;
}

static void
_uv_on_walk_cb(uv_handle_t *handle, void *arg)
{
  uv_close(handle, NULL);

  (void)arg;
}

static void
_concord_http_destroy(concord_http_t *http)
{
  curl_slist_free_all(http->request_header);
  curl_multi_cleanup(http->multi_handle);

  int uvcode = uv_loop_close(http->loop);
  if (UV_EBUSY == uvcode){ //there are still handles that need to be closed
    uv_walk(http->loop, &_uv_on_walk_cb, NULL); //close each handle encountered

    uvcode = uv_run(http->loop, UV_RUN_DEFAULT); //run the loop again to close remaining handles
    DEBUG_ASSERT(!uvcode, uv_strerror(uvcode));

    uvcode = uv_loop_close(http->loop); //finally, close the loop
    DEBUG_ASSERT(!uvcode, uv_strerror(uvcode));
  }

  dictionary_destroy(http->bucket_dict);
  dictionary_destroy(http->header);

  safe_free(http->client_buckets);

  safe_free(http->token);

  safe_free(http);
}

concord_t*
concord_init(char token[])
{
  concord_t *new_concord = safe_calloc(1, sizeof *new_concord);

  new_concord->http = _concord_http_init(token);
  new_concord->ws = Concord_ws_init(token);

  new_concord->channel = concord_channel_init();
  new_concord->guild = concord_guild_init();
  new_concord->user = concord_user_init();
  new_concord->client = concord_user_init();

  return new_concord;
}

void
concord_cleanup(concord_t *concord)
{
  Concord_ws_destroy(concord->ws);

  _concord_http_destroy(concord->http);

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
