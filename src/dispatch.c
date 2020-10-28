#include <stdio.h>
#include <stdlib.h>

#include <libconcord.h>

#include "hashtable.h"
#include "debug.h"
#include "ratelimit.h"
#include "dispatch.h"


static concord_context_st*
_concord_context_init(concord_utils_st *utils, curl_socket_t sockfd)
{
  debug_puts("Creating new context");
  concord_context_st *new_context = safe_malloc(sizeof *new_context);

  new_context->sockfd = sockfd;

  int uvcode = uv_poll_init_socket(utils->loop, &new_context->poll_handle, sockfd);
  debug_assert(!uvcode, uv_strerror(uvcode));

  new_context->poll_handle.data = utils;

  return new_context;
}

static void
_curl_close_cb(uv_handle_t *handle)
{
  debug_puts("Destroying context");
  concord_context_st *context = (concord_context_st*)handle;
  safe_free(context);
}

static void
_concord_context_destroy(concord_context_st *context)
{
  uv_close((uv_handle_t*)&context->poll_handle, &_curl_close_cb);
}

void
_concord_queue_update(uv_timer_t *req)
{
  debug_puts("Updating bucket queue");
  struct concord_bucket_s *bucket = req->data;

  do {
    Concord_queue_pop(bucket->p_utils, bucket);
  } while (bucket->remaining--);
}

static void
_concord_tryupdate_queue(concord_utils_st *utils)
{
  /* See how the transfers went */
  CURLMsg *msg; /* for picking up messages with the transfer status */
  int pending; /*how many messages are left */
  long http_code; /* http response code */
  while ((msg = curl_multi_info_read(utils->multi_handle, &pending)))
  {
    if (CURLMSG_DONE != msg->msg)
      continue;

    debug_print("Transfers Running: %d\n\tTransfers On Hold: %d", utils->transfers_running, utils->transfers_onhold);

    /* Find out which handle this message is about */
    char easy_key[18];
    sprintf(easy_key, "%p", msg->easy_handle);
    struct concord_conn_s *conn = dictionary_get(utils->easy_dict, easy_key);

    curl_easy_getinfo(conn->easy_handle, CURLINFO_RESPONSE_CODE, &http_code);
    debug_assert(429 != http_code, "Being ratelimited");

    /* execute load callback to perform change in object */
    if (NULL != conn->response_body.str){
      //debug_puts(conn->response_body.str);
      (*conn->load_cb)(conn->p_object, &conn->response_body);

      conn->p_object = NULL;

      safe_free(conn->response_body.str);
      conn->response_body.size = 0;
    }

    curl_multi_remove_handle(utils->multi_handle, conn->easy_handle);
    
    conn->p_bucket->remaining = dictionary_get_strtoll(utils->header, "x-ratelimit-remaining");
    debug_print("Ratelimit remaining: %d", conn->p_bucket->remaining);

    long long delay_ms;
    if (0 == conn->p_bucket->remaining){
      delay_ms = Concord_parse_ratelimit_header(utils->header, true);
    } else {
      delay_ms = 0;
    }
    
    uv_timer_start(&conn->p_bucket->timer, &_concord_queue_update, delay_ms, 0);
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
  debug_assert(CURLM_OK == mcode, curl_multi_strerror(mcode));

  _concord_tryupdate_queue(utils);
}

static void
_uv_on_timeout_cb(uv_timer_t *req)
{
  concord_utils_st *utils = req->data;
  debug_assert(NULL != utils, "Handle data is empty");

  CURLMcode mcode = curl_multi_socket_action(utils->multi_handle, CURL_SOCKET_TIMEOUT, 0, &utils->transfers_running);
  debug_assert(CURLM_OK == mcode, curl_multi_strerror(mcode));

  _concord_tryupdate_queue(utils);
}

int
Curl_start_timeout_cb(CURLM *multi_handle, long timeout_ms, void *p_userdata)
{
  uv_timer_t *timeout = p_userdata;
  debug_assert(NULL != timeout, "Timeout handle unspecified");

  if (timeout_ms < 0){
    int uvcode = uv_timer_stop(timeout);
    debug_assert(!uvcode, uv_strerror(uvcode));
  } else {
    if (0 == timeout_ms)
      timeout_ms = 1;

    int uvcode = uv_timer_start(timeout, &_uv_on_timeout_cb, timeout_ms, 0);
    debug_assert(!uvcode, uv_strerror(uvcode));
  }

  return 0;
}

int
Curl_handle_socket_cb(CURL *easy_handle, curl_socket_t sockfd, int action, void *p_userdata, void *p_socket)
{
  concord_utils_st *utils = p_userdata;
  CURLMcode mcode;
  int uvcode;

  concord_context_st *context;
  if (action == CURL_POLL_IN || action == CURL_POLL_OUT){
    if (NULL != p_socket){
      context = (concord_context_st*)p_socket;
    } else {
      context = _concord_context_init(utils, sockfd);
    }

    mcode = curl_multi_assign(utils->multi_handle, sockfd, context);
    debug_assert(CURLM_OK == mcode, curl_multi_strerror(mcode));
  }

  switch (action){
  case CURL_POLL_IN:
      debug_puts("POLL IN FLAG");
      uvcode = uv_poll_start(&context->poll_handle, UV_READABLE, &_uv_perform_cb);
      debug_assert(!uvcode, uv_strerror(uvcode));
      break;
  case CURL_POLL_OUT:
      debug_puts("POLL OUT FLAG");
      uvcode = uv_poll_start(&context->poll_handle, UV_WRITABLE, &_uv_perform_cb);
      debug_assert(!uvcode, uv_strerror(uvcode));
      break;
  case CURL_POLL_REMOVE:
      debug_puts("POLL REMOVE FLAG");
      if (p_socket){
        uvcode = uv_poll_stop(&((concord_context_st*)p_socket)->poll_handle);
        debug_assert(!uvcode, uv_strerror(uvcode));

        _concord_context_destroy((concord_context_st*)p_socket);

        mcode = curl_multi_assign(utils->multi_handle, sockfd, NULL);
        debug_assert(CURLM_OK == mcode, curl_multi_strerror(mcode));
      }
      break;
  default:
      debug_puts("An error has occurred");
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
  debug_assert(!uvcode, uv_strerror(uvcode));
  
  debug_assert(0 == utils->transfers_onhold, "Left loop with pending transfers");

  Concord_reset_client_buckets(utils);
}
