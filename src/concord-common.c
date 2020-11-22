#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <libconcord.h>
#include "concord-common.h"

#include "hashtable.h"
#include "debug.h"


struct concord_context_s*
Concord_context_init(uv_loop_t *loop, curl_socket_t sockfd)
{
  DEBUG_NOTOP_PUTS("Creating new context");
  struct concord_context_s *new_context = safe_calloc(1, sizeof *new_context);

  new_context->sockfd = sockfd;

  int uvcode = uv_poll_init_socket(loop, &new_context->poll_handle, sockfd);
  DEBUG_ASSERT(!uvcode, uv_strerror(uvcode));

  uv_handle_set_data((uv_handle_t*)&new_context->poll_handle, uv_loop_get_data(loop));

  return new_context;
}

static void
_uv_context_destroy_cb(uv_handle_t *handle)
{
  DEBUG_NOTOP_PUTS("Destroying context");
  struct concord_context_s *context = (struct concord_context_s*)handle;
  safe_free(context);
}

void
Concord_context_destroy(struct concord_context_s *context)
{
  uv_close((uv_handle_t*)&context->poll_handle, &_uv_context_destroy_cb);
}


void
Concord_api_request(
  concord_api_t *api, 
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
             api,
             p_object,
             load_cb,
             http_method,
             bucket_key,
             url_route);
}

static concord_api_t*
_concord_api_init(char token[])
{
  concord_api_t *new_api = safe_calloc(1, sizeof *new_api);

  new_api->loop = uv_default_loop();
  uv_loop_set_data(new_api->loop, new_api);

  new_api->token = strndup(token, strlen(token)-1);
  DEBUG_ASSERT(NULL != new_api->token, "Out of memory");

  new_api->request_header = Curl_request_header_init(new_api);

  uv_timer_init(new_api->loop, &new_api->timeout);
  uv_handle_set_data((uv_handle_t*)&new_api->timeout, new_api);

  new_api->multi_handle = Concord_api_multi_init(new_api);

  new_api->bucket_dict = dictionary_init();
  dictionary_build(new_api->bucket_dict, BUCKET_DICTIONARY_SIZE);

  new_api->header = dictionary_init();
  dictionary_build(new_api->header, HEADER_DICTIONARY_SIZE);

  return new_api;
}

static void
_uv_on_walk_cb(uv_handle_t *handle, void *arg)
{
  uv_close(handle, NULL);

  (void)arg;
}

static void
_concord_api_destroy(concord_api_t *api)
{
  curl_slist_free_all(api->request_header);
  curl_multi_cleanup(api->multi_handle);

  int uvcode = uv_loop_close(api->loop);
  if (UV_EBUSY == uvcode){ //there are still handles that need to be closed
    uv_walk(api->loop, &_uv_on_walk_cb, NULL); //close each handle encountered

    uvcode = uv_run(api->loop, UV_RUN_DEFAULT); //run the loop again to close remaining handles
    DEBUG_ASSERT(!uvcode, uv_strerror(uvcode));

    uvcode = uv_loop_close(api->loop); //finally, close the loop
    DEBUG_ASSERT(!uvcode, uv_strerror(uvcode));
  }

  dictionary_destroy(api->bucket_dict);
  dictionary_destroy(api->header);

  safe_free(api->client_buckets);

  safe_free(api->token);

  safe_free(api);
}

concord_t*
concord_init(char token[])
{
  concord_t *new_concord = safe_calloc(1, sizeof *new_concord);

  new_concord->api = _concord_api_init(token);
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
  _concord_api_destroy(concord->api);

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
