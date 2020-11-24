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

concord_t*
concord_init(char token[])
{
  concord_t *new_concord = safe_calloc(1, sizeof *new_concord);

  new_concord->api = Concord_api_init(token);
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
  Concord_api_destroy(concord->api);

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
