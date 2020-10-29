#include <stdio.h>
#include <stdlib.h>

#include <libconcord.h>
#include "concord-common.h"

#include "hashtable.h"
#include "debug.h"


static concord_context_st*
_concord_context_init(concord_utils_st *utils, curl_socket_t sockfd)
{
  DEBUG_PUTS("Creating new context");
  concord_context_st *new_context = safe_malloc(sizeof *new_context);

  new_context->sockfd = sockfd;

  int uvcode = uv_poll_init_socket(utils->loop, &new_context->poll_handle, sockfd);
  DEBUG_ASSERT(!uvcode, uv_strerror(uvcode));

  new_context->poll_handle.data = utils;

  return new_context;
}

static void
_uv_context_destroy_cb(uv_handle_t *handle)
{
  DEBUG_PUTS("Destroying context");
  concord_context_st *context = (concord_context_st*)handle;
  safe_free(context);
}

static void
_concord_context_destroy(concord_context_st *context)
{
  uv_close((uv_handle_t*)&context->poll_handle, &_uv_context_destroy_cb);
}

static void
_uv_add_remaining_cb(uv_timer_t *req)
{
  DEBUG_PUTS("Updating bucket queue");
  struct concord_bucket_s *bucket = req->data;

  do {
    Concord_queue_pop(bucket->p_utils, bucket);
  } while (bucket->remaining--);
}

/* if is global, then sleep for x amount inside the function and
    return 0, otherwise return the retry_after amount in ms */
static long long
_concord_429_handle(struct concord_response_s *response_body)
{
  DEBUG_PUTS("Being ratelimited");

  jscon_item_st *item = jscon_parse(response_body->str);

  bool global = jscon_get_boolean(jscon_get_branch(item, "global"));
  long long retry_after = jscon_get_double(jscon_get_branch(item, "retry_after"));

  if (global == true){
    DEBUG_PRINT("Global ratelimit, retrying after %lld seconds", retry_after);
    uv_sleep(retry_after * 1000);

    retry_after = 0;
  }

  jscon_destroy(item);

  return retry_after * 1000;
}

static void
_concord_conn_response_perform(concord_utils_st *utils, struct concord_conn_s *conn)
{
  enum discord_http_code http_code; /* http response code */
  char *url = NULL; /* URL from request */
  
  CURLcode ecode;
  ecode = curl_easy_getinfo(conn->easy_handle, CURLINFO_RESPONSE_CODE, &http_code);
  DEBUG_ASSERT(CURLE_OK == ecode, curl_easy_strerror(ecode));

  ecode = curl_easy_getinfo(conn->easy_handle, CURLINFO_EFFECTIVE_URL, &url);
  DEBUG_ASSERT(CURLE_OK == ecode, curl_easy_strerror(ecode));

  DEBUG_PRINT("Conn URL: %s", url);

  long long delay_ms;
  switch (http_code){
  case DISCORD_OK: /* 200 */
      (*conn->load_cb)(conn->p_object, &conn->response_body);

      conn->p_object = NULL;

      safe_free(conn->response_body.str);
      conn->response_body.size = 0;

      conn->p_bucket->remaining = dictionary_get_strtoll(utils->header, "x-ratelimit-remaining");
      DEBUG_PRINT("Ratelimit remaining: %d", conn->p_bucket->remaining);

      if (!conn->p_bucket->remaining){
        delay_ms = Concord_parse_ratelimit_header(utils->header, true);
      } else {
        delay_ms = 0;
      }
      break;
  case DISCORD_TOO_MANY_REQUESTS: /* 429 */
      delay_ms = _concord_429_handle(&conn->response_body);
      break;
  case CURL_NO_RESPONSE: /* 0 */
      /* @todo look into this, for some reason sometimes the easy handle
          has an empty url, which gives us a 0 http_code response */
      // !url means URL string is NULL , !*url means URL string is empty
      DEBUG_ASSERT(!url || !*url, "No server response has been received");
      return;
  default:
      DEBUG_PRINT("HTTP CODE: %d", http_code);
      abort();
  }
  
  /* after delay_ms time has elapsed, the event loop will add the
      remaining connections to the multi stack (if there are any) */
  uv_timer_start(&conn->p_bucket->timer, &_uv_add_remaining_cb, delay_ms, 0);
}

static void
_concord_tryperform_response(concord_utils_st *utils)
{
  /* These are related to the current easy_handle being read */
  CURLMsg *msg; /* for picking up messages with the transfer status */
  int pending; /*how many messages are left */

  /* search for completed easy_handle transfers, and perform the
      instructions given by the transfer response */
  while ((msg = curl_multi_info_read(utils->multi_handle, &pending)))
  {
    if (CURLMSG_DONE != msg->msg)
      continue;

    DEBUG_PRINT("Transfers Running: %d\n\tTransfers On Hold: %d", utils->transfers_running, utils->transfers_onhold);

    /* Find out which handle this message is about */
    char easy_key[18];
    sprintf(easy_key, "%p", msg->easy_handle);
    struct concord_conn_s *conn = dictionary_get(utils->easy_dict, easy_key);

    curl_multi_remove_handle(utils->multi_handle, conn->easy_handle);

    _concord_conn_response_perform(utils, conn);
  }
}

static void
_uv_perform_cb(uv_poll_t *req, int status, int events)
{
  concord_utils_st *utils = req->data;
  concord_context_st *context = (concord_context_st*)req;

  int flags = 0;
  if (events & UV_READABLE) flags |= CURL_CSELECT_IN;
  if (events & UV_WRITABLE) flags |= CURL_CSELECT_OUT;

  CURLMcode mcode = curl_multi_socket_action(utils->multi_handle, context->sockfd, flags, &utils->transfers_running);
  DEBUG_ASSERT(CURLM_OK == mcode, curl_multi_strerror(mcode));

  _concord_tryperform_response(utils);
}

static void
_uv_on_timeout_cb(uv_timer_t *req)
{
  concord_utils_st *utils = req->data;

  CURLMcode mcode = curl_multi_socket_action(utils->multi_handle, CURL_SOCKET_TIMEOUT, 0, &utils->transfers_running);
  DEBUG_ASSERT(CURLM_OK == mcode, curl_multi_strerror(mcode));

  _concord_tryperform_response(utils);
}

int
Curl_start_timeout_cb(CURLM *multi_handle, long timeout_ms, void *p_userdata)
{
  uv_timer_t *timeout = p_userdata;

  if (timeout_ms < 0){
    int uvcode = uv_timer_stop(timeout);
    DEBUG_ASSERT(!uvcode, uv_strerror(uvcode));
  } else {
    if (0 == timeout_ms)
      timeout_ms = 1;

    int uvcode = uv_timer_start(timeout, &_uv_on_timeout_cb, timeout_ms, 0);
    DEBUG_ASSERT(!uvcode, uv_strerror(uvcode));
  }

  return 0;
}

int
Curl_handle_socket_cb(CURL *easy_handle, curl_socket_t sockfd, int action, void *p_userdata, void *p_socket)
{
  concord_utils_st *utils = p_userdata;
  CURLMcode mcode;
  int uvcode;
  int events = 0;
  concord_context_st *context;


  switch (action){
  case CURL_POLL_IN:
  case CURL_POLL_OUT:
      if (NULL != p_socket){
        context = (concord_context_st*)p_socket;
      } else {
        context = _concord_context_init(utils, sockfd);
      }

      mcode = curl_multi_assign(utils->multi_handle, sockfd, context);
      DEBUG_ASSERT(CURLM_OK == mcode, curl_multi_strerror(mcode));

      if (action != CURL_POLL_IN) events |= UV_WRITABLE;
      if (action != CURL_POLL_OUT) events |= UV_READABLE;

      uvcode = uv_poll_start(&context->poll_handle, events, &_uv_perform_cb);
      DEBUG_ASSERT(!uvcode, uv_strerror(uvcode));
      break;
  case CURL_POLL_REMOVE:
      if (NULL != p_socket){
        uvcode = uv_poll_stop(&((concord_context_st*)p_socket)->poll_handle);
        DEBUG_ASSERT(!uvcode, uv_strerror(uvcode));

        _concord_context_destroy((concord_context_st*)p_socket);

        mcode = curl_multi_assign(utils->multi_handle, sockfd, NULL);
        DEBUG_ASSERT(CURLM_OK == mcode, curl_multi_strerror(mcode));
      }
      break;
  default:
      DEBUG_PRINT("Unknown CURL_POLL_XXX option encountered\n\tCode: %d", action);
      abort();
  }

  return 0;
}

void
concord_dispatch(concord_st *concord)
{
  concord_utils_st *utils = &concord->utils;

  Concord_start_client_buckets(utils);

  int uvcode = uv_run(utils->loop, UV_RUN_DEFAULT);
  DEBUG_ASSERT(!uvcode, uv_strerror(uvcode));
  
  DEBUG_ASSERT(!utils->transfers_onhold, "Left loop with pending transfers");
  DEBUG_ASSERT(!utils->transfers_running, "Left loop with running transfers");

  Concord_stop_client_buckets(utils);
}

void
Concord_synchronous_perform(concord_utils_st *utils, struct concord_conn_s *conn)
{
  CURLcode ecode = curl_easy_perform(conn->easy_handle);
  DEBUG_ASSERT(CURLE_OK == ecode, curl_easy_strerror(ecode));

  if (NULL != conn->response_body.str){
    int remaining = dictionary_get_strtoll(utils->header, "x-ratelimit-remaining");
    DEBUG_PRINT("Remaining connections: %d", remaining);
    if (0 == remaining){
      long long delay_ms = Concord_parse_ratelimit_header(utils->header, false);
      DEBUG_PRINT("Delay_ms: %lld", delay_ms);
      uv_sleep(delay_ms);
    }

    //DEBUG_PUTS(conn->response_body.str);
    (*conn->load_cb)(conn->p_object, &conn->response_body);

    conn->p_object = NULL;

    safe_free(conn->response_body.str);
    conn->response_body.size = 0;
  }
}
