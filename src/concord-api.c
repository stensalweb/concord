#include <stdio.h>
#include <stdlib.h>

#include <libconcord.h>
#include "concord-common.h"

#include "hashtable.h"
#include "debug.h"


struct concord_context_s*
Concord_context_init(uv_loop_t *loop, curl_socket_t sockfd)
{
  DEBUG_NOTOP_PUTS("Creating new context");
  struct concord_context_s *new_context = safe_malloc(sizeof *new_context);

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

static void
_uv_add_remaining_cb(uv_timer_t *req)
{
  struct concord_bucket_s *bucket = uv_handle_get_data((uv_handle_t*)req);

  Concord_queue_npop(bucket->p_http, &bucket->queue, bucket->remaining);
  bucket->finished = bucket->remaining = 0;
}

static void
_concord_load_obj_perform(struct concord_conn_s *conn)
{
  (*conn->load_cb)(conn->p_object, &conn->response_body);
  DEBUG_NOTOP_PUTS("Object loaded with API response"); 

  conn->p_object = NULL;
  conn->load_cb  = NULL;

  safe_free(conn->response_body.str);
  conn->response_body.size = 0;

  conn->status = INNACTIVE;
}

static void
_concord_200async_tryremaining(concord_http_st *http, struct concord_conn_s *conn)
{
  _concord_load_obj_perform(conn);
  curl_multi_remove_handle(http->multi_handle, conn->easy_handle);

  struct concord_bucket_s *bucket = conn->p_bucket;
  if (bucket->queue.separator == bucket->queue.top_onhold){
    DEBUG_PRINT("Bucket Hash:\t%s\n\t" \
                "No conn remaining in queue to be added",
                bucket->hash_key);
    return;
  }

  /* if conn->p_bucket->finished is greater than remaining, then
   *   we can fetch more connections from queue (if there are any) */
  if (bucket->finished == bucket->remaining){
    long long delay_ms = Concord_parse_ratelimit_header(bucket, http->header, true);
    /* after delay_ms time has elapsed, the event loop will add the
     *   remaining connections to the multi stack (if there are any) */
    int uvcode = uv_timer_start(&bucket->ratelimit_timer, &_uv_add_remaining_cb, delay_ms, 0);
    DEBUG_ASSERT(!uvcode, uv_strerror(uvcode));
  }

  ++bucket->finished;

  DEBUG_PRINT("Finished transfers:\t%d\n\t" \
              "Remainining transfers:\t%d",
              bucket->finished,
              bucket->remaining);
}

static void
_concord_queue_pause(struct concord_queue_s *queue)
{
  concord_http_st *http = ((struct concord_bucket_s*)queue)->p_http;

  for (size_t i = queue->bottom_running; i < queue->separator; ++i){
    if (RUNNING != queue->conns[i]->status)
      continue;

    curl_multi_remove_handle(http->multi_handle, queue->conns[i]->easy_handle);
    queue->conns[i]->status = PAUSE; 
  }
}

static void
_uv_queue_resume_cb(uv_timer_t *req)
{
  struct concord_queue_s *queue = uv_handle_get_data((uv_handle_t*)req);
  concord_http_st *http = ((struct concord_bucket_s*)queue)->p_http;


  DEBUG_PRINT("Resuming pending transfers:\t%ld", uv_now(http->loop));

  for (size_t i = queue->bottom_running; i < queue->separator; ++i){
    if (PAUSE != queue->conns[i]->status)
      continue;

    curl_multi_add_handle(http->multi_handle, queue->conns[i]->easy_handle);
    queue->conns[i]->status = RUNNING; 
  }
}

/* if is global, then sleep for x amount inside the function and
    return 0, otherwise return the retry_after amount in ms */
static void
_concord_429async_tryrecover(concord_http_st *http, struct concord_conn_s *conn)
{
  char message[256] = {0};
  long long retry_after;
  bool global;


  jscon_scanf(conn->response_body.str,
    "#message%ls " \
    "#retry_after%jd " \
    "#global%jb",
     message,
     &retry_after,
     &global);

  DEBUG_PRINT("%s", message);

  if (true == global){
    DEBUG_NOTOP_PRINT("Global ratelimit, retrying after %lld seconds", retry_after);
    uv_sleep(retry_after*1000);

    curl_multi_remove_handle(http->multi_handle, conn->easy_handle);
    curl_multi_add_handle(http->multi_handle, conn->easy_handle);
  }
  else {
    DEBUG_NOTOP_PRINT("Bucket ratelimit, stopping transfer on hold for %lld seconds", retry_after);
    uv_timer_start(&conn->p_bucket->ratelimit_timer, &_uv_queue_resume_cb, retry_after*1000, 0);
    _concord_queue_pause(&conn->p_bucket->queue);
  }

  safe_free(conn->response_body.str);
  conn->response_body.size = 0;
}

static void
_concord_asynchronous_perform(concord_http_st *http, CURL *easy_handle)
{
  struct concord_conn_s *conn; /* conn referenced by this easy_handle */
  enum discord_http_code http_code; /* http response code */
  char *url = NULL; /* URL from request */
  CURLcode ecode;


  ecode = curl_easy_getinfo(easy_handle, CURLINFO_RESPONSE_CODE, &http_code);
  DEBUG_ASSERT(CURLE_OK == ecode, curl_easy_strerror(ecode));

  ecode = curl_easy_getinfo(easy_handle, CURLINFO_PRIVATE, &conn);
  DEBUG_ASSERT(CURLE_OK == ecode, curl_easy_strerror(ecode));

  ecode = curl_easy_getinfo(easy_handle, CURLINFO_EFFECTIVE_URL, &url);
  DEBUG_ASSERT(CURLE_OK == ecode, curl_easy_strerror(ecode));

  DEBUG_PRINT("Conn URL: %s\n\t" \
              "Transfers Running: %d\n\t" \
              "Transfers On Hold: %d",
              url,
              http->transfers_running,
              http->transfers_onhold);
    

  switch (http_code){
  case HTTP_OK:
      _concord_200async_tryremaining(http, conn);
      return;
  case HTTP_TOO_MANY_REQUESTS:
      _concord_429async_tryrecover(http, conn);
      return;
  case CURL_NO_RESPONSE: 
      DEBUG_ASSERT(!url || !*url, "No server response has been received");
      curl_multi_remove_handle(http->multi_handle, easy_handle);
      return;
  default:
      DEBUG_ERR("Found not yet implemented HTTP Code: %d", http_code);
  }
}

static void
_concord_tryperform_asynchronous(concord_http_st *http)
{
  /* These are related to the current easy_handle being read */
  CURLMsg *msg; /* for picking up messages with the transfer status */
  int pending; /*how many messages are left */

  /* search for completed easy_handle transfers, and perform the
      instructions given by the transfer response */
  while ((msg = curl_multi_info_read(http->multi_handle, &pending)))
  {
    if (CURLMSG_DONE != msg->msg)
      continue;
    
    _concord_asynchronous_perform(http, msg->easy_handle);
  }
}

static void
_uv_perform_cb(uv_poll_t *req, int uvstatus, int events)
{
  DEBUG_ASSERT(!uvstatus, uv_strerror(uvstatus));

  concord_http_st *http = uv_handle_get_data((uv_handle_t*)req);
  struct concord_context_s *context = (struct concord_context_s*)req;

  int flags = 0;
  if (events & UV_READABLE) flags |= CURL_CSELECT_IN;
  if (events & UV_WRITABLE) flags |= CURL_CSELECT_OUT;

  CURLMcode mcode = curl_multi_socket_action(http->multi_handle, context->sockfd, flags, &http->transfers_running);
  DEBUG_ASSERT(CURLM_OK == mcode, curl_multi_strerror(mcode));

  _concord_tryperform_asynchronous(http);
}

static void
_uv_on_timeout_cb(uv_timer_t *req)
{
  concord_http_st *http = uv_handle_get_data((uv_handle_t*)req);

  CURLMcode mcode = curl_multi_socket_action(http->multi_handle, CURL_SOCKET_TIMEOUT, 0, &http->transfers_running);
  DEBUG_ASSERT(CURLM_OK == mcode, curl_multi_strerror(mcode));
}

int
Concord_http_timeout_cb(CURLM *multi_handle, long timeout_ms, void *p_userdata)
{
  uv_timer_t *timeout = p_userdata;
  int uvcode;

  if (timeout_ms < 0){
    uvcode = uv_timer_stop(timeout);
    DEBUG_ASSERT(!uvcode, uv_strerror(uvcode));
  } else {
    if (0 == timeout_ms){
      timeout_ms = 1;
    }

    uvcode = uv_timer_start(timeout, &_uv_on_timeout_cb, timeout_ms, 0);
    DEBUG_ASSERT(!uvcode, uv_strerror(uvcode));
  }

  (void)multi_handle;

  return 0;
}

int
Concord_http_socket_cb(CURL *easy_handle, curl_socket_t sockfd, int action, void *p_userdata, void *p_socket)
{
  concord_http_st *http = p_userdata;
  CURLMcode mcode;
  int uvcode;
  int events = 0;
  struct concord_conn_s *conn;

  if (!p_socket){
    CURLcode ecode = curl_easy_getinfo(easy_handle, CURLINFO_PRIVATE, &conn);
    DEBUG_ASSERT(CURLE_OK == ecode, curl_easy_strerror(ecode));

    mcode = curl_multi_assign(http->multi_handle, sockfd, conn);
    DEBUG_ASSERT(CURLM_OK == mcode, curl_multi_strerror(mcode));
  } else {
    conn = p_socket;
  }

  switch (action){
  case CURL_POLL_IN:
  case CURL_POLL_OUT:
      if (!conn->context){
        conn->context = Concord_context_init(http->loop, sockfd);
      }

      if (action != CURL_POLL_IN) events |= UV_WRITABLE;
      if (action != CURL_POLL_OUT) events |= UV_READABLE;

      uvcode = uv_poll_start(&conn->context->poll_handle, events, &_uv_perform_cb);
      DEBUG_ASSERT(!uvcode, uv_strerror(uvcode));
      break;
  case CURL_POLL_REMOVE:
      if (conn){
        uvcode = uv_poll_stop(&conn->context->poll_handle);
        DEBUG_ASSERT(!uvcode, uv_strerror(uvcode));

        Concord_context_destroy(conn->context);
        conn->context = NULL;

        mcode = curl_multi_assign(http->multi_handle, sockfd, NULL);
        DEBUG_ASSERT(CURLM_OK == mcode, curl_multi_strerror(mcode));
      }
      break;
  default:
      DEBUG_ERR("Unknown CURL_POLL_XXXX option encountered\n\tCode: %d", action);
  }

  return 0;
}

void
Concord_transfers_run(concord_http_st *http)
{
  if (!http->transfers_onhold){
    DEBUG_NOTOP_PUTS("No transfers on hold, returning ..."); 
    return;
  }

  /* kickstart transfers by sending first connection of each bucket */
  Concord_start_client_buckets(http);

  int uvcode = uv_run(http->loop, UV_RUN_DEFAULT);
  DEBUG_ASSERT(!uvcode, uv_strerror(uvcode));
  
  DEBUG_ASSERT(!http->transfers_onhold, "Left loop with pending transfers");
  DEBUG_ASSERT(!http->transfers_running, "Left loop with running transfers");

  /* set each bucket attribute to zero */
  Concord_stop_client_buckets(http);
}

void
concord_dispatch(concord_st *concord)
{
  Concord_transfers_run(concord->http);
}

static void
_concord_200sync_getbucket(concord_http_st *http, struct concord_conn_s *conn, char bucket_key[])
{
  long long delay_ms = Concord_parse_ratelimit_header(NULL, http->header, true);
  uv_sleep(delay_ms);

  _concord_load_obj_perform(conn);

  char *bucket_hash = dictionary_get(http->header, "x-ratelimit-bucket");
  DEBUG_NOTOP_PRINT("New Key/Hash pair:  %s - %s", bucket_key, bucket_hash);

  /* create bucket if it doesn't exist, otherwise, get existing one */
  struct concord_bucket_s *bucket = Concord_trycreate_bucket(http, bucket_hash);

  /* try to find a empty bucket queue slot */
  size_t empty_slot = bucket->queue.top_onhold;
  while (NULL != bucket->queue.conns[empty_slot]){
    ++empty_slot;
  }
  DEBUG_ASSERT(empty_slot < bucket->queue.size, "Queue has reached its threshold");

  bucket->queue.conns[empty_slot] = conn; /* insert conn in empty slot */
  conn->p_bucket = bucket; /* save reference bucket from conn */

  void *res = dictionary_set(http->bucket_dict, bucket_key, bucket, NULL);
  DEBUG_ASSERT(res == bucket, "Can't link bucket key with an existing bucket");
}

/* for synchronous 429, we won't be dealing with global and non-global ratelimits
    separately, both will block all connections the same. This shouldn't affect
    performance, as synchronous transfers are only performed when trying to
    match a first time seen bucket_key with a bucket, so getting ratelimited in a synchronous transfer would be a rare occurrence */
static void
_concord_429sync_tryrecover(struct concord_conn_s *conn)
{
  char message[256] = {0};
  long long retry_after;

  jscon_scanf(conn->response_body.str,
    "#message%ls " \
    "#retry_after%jd",
     message,
     &retry_after);

  DEBUG_PRINT("%s", message);

  uv_sleep(retry_after*1000);

  safe_free(conn->response_body.str);
  conn->response_body.size = 0;
}

void
Concord_register_bucket_key(concord_http_st *http, struct concord_conn_s *conn, char bucket_key[])
{
  enum discord_http_code http_code; /* http response code */
  char *url = NULL; /* URL from request */
  CURLcode ecode;


  do {
      ecode = curl_easy_perform(conn->easy_handle);
      DEBUG_ASSERT(CURLE_OK == ecode, curl_easy_strerror(ecode));

      ecode = curl_easy_getinfo(conn->easy_handle, CURLINFO_RESPONSE_CODE, &http_code);
      DEBUG_ASSERT(CURLE_OK == ecode, curl_easy_strerror(ecode));

      ecode = curl_easy_getinfo(conn->easy_handle, CURLINFO_EFFECTIVE_URL, &url);
      DEBUG_ASSERT(CURLE_OK == ecode, curl_easy_strerror(ecode));

      DEBUG_NOTOP_PRINT("Conn URL: %s", url);

      switch (http_code){
      case HTTP_OK:
          _concord_200sync_getbucket(http, conn, bucket_key);
          return; /* DONE */
      case HTTP_TOO_MANY_REQUESTS:
          _concord_429sync_tryrecover(conn);
          break;
      case CURL_NO_RESPONSE: 
          DEBUG_ASSERT(!url || !*url, "No server response has been received");
          return; /* early exit */
      default:
          DEBUG_ERR("Found not yet implemented HTTP Code: %d", http_code);
      }
  } while (HTTP_OK != http_code);
}
