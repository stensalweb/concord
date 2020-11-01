#include <stdio.h>
#include <stdlib.h>

#include <libconcord.h>
#include "concord-common.h"

#include "hashtable.h"
#include "debug.h"


static struct concord_context_s*
_concord_context_init(concord_utils_st *utils, curl_socket_t sockfd)
{
  DEBUG_PUTS("Creating new context");
  struct concord_context_s *new_context = safe_malloc(sizeof *new_context);

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
  struct concord_context_s *context = (struct concord_context_s*)handle;
  safe_free(context);
}

static void
_concord_context_destroy(struct concord_context_s *context)
{
  uv_close((uv_handle_t*)&context->poll_handle, &_uv_context_destroy_cb);
}

/* @todo move to concord-ratelimit.c */
static void
_uv_add_remaining_cb(uv_timer_t *req)
{
  DEBUG_PUTS("Adding remainining conns");
  struct concord_bucket_s *bucket = req->data;

  bucket->queue.bottom_running = bucket->queue.separator;

  do {
    Concord_queue_pop(bucket->p_utils, &bucket->queue);
  } while (bucket->remaining--);

  DEBUG_PRINT("Bucket Hash:\t%s\n\t" \
              "Queue Size:\t%ld\n\t" \
              "Queue Bottom:\t%ld\n\t" \
              "Queue Separator:%ld\n\t" \
              "Queue Top:\t%ld",
              bucket->hash_key,
              bucket->queue.size,
              bucket->queue.bottom_running,
              bucket->queue.separator,
              bucket->queue.top_onhold);
}

static void
_concord_load_obj_perform(struct concord_conn_s *conn)
{
  (*conn->load_cb)(conn->p_object, &conn->response_body);

  conn->p_object = NULL;

  safe_free(conn->response_body.str);
  conn->response_body.size = 0;
}

static void
_concord_200_async_action(concord_utils_st *utils, struct concord_conn_s *conn)
{
  long long delay_ms = Concord_parse_ratelimit_header(conn->p_bucket, utils->header, true);
  /* after delay_ms time has elapsed, the event loop will add the remaining connections to the multi stack (if there are any) */
  uv_timer_start(&conn->p_bucket->timer, &_uv_add_remaining_cb, delay_ms, 0);

  _concord_load_obj_perform(conn);

  curl_multi_remove_handle(utils->multi_handle, conn->easy_handle);
  conn->status = INNACTIVE;
}

static void
_concord_queue_pause(struct concord_queue_s *queue)
{
  concord_utils_st *utils = ((struct concord_bucket_s*)queue)->p_utils;


  /* @todo this is not an optimal solution, doing transfer status
      per bucket instead of per connection would be quicker */
  for (size_t i = queue->bottom_running; i < queue->separator; ++i){
    if (RUNNING != queue->conns[i]->status)
      continue;

    curl_multi_remove_handle(utils->multi_handle, queue->conns[i]->easy_handle);
    queue->conns[i]->status = PAUSE; 
  }
}

static void
_uv_queue_resume_cb(uv_timer_t *req)
{
  struct concord_queue_s *queue = req->data;
  concord_utils_st *utils = ((struct concord_bucket_s*)queue)->p_utils;


  /* @todo this is not an optimal solution, doing transfer status
      per bucket instead of per connection would be quicker */
  for (size_t i = queue->bottom_running; i < queue->separator; ++i){
    if (PAUSE != queue->conns[i]->status)
      continue;

    curl_multi_add_handle(utils->multi_handle, queue->conns[i]->easy_handle);
    queue->conns[i]->status = RUNNING; 
  }
}

/* if is global, then sleep for x amount inside the function and
    return 0, otherwise return the retry_after amount in ms */
static void
_concord_429_async_action(concord_utils_st *utils, struct concord_conn_s *conn)
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

  DEBUG_PRINT("Being ratelimited:\t%s", message);

  if (true == global){
    DEBUG_PRINT("Global ratelimit, retrying after %lld seconds", retry_after);
    uv_sleep(retry_after*1000);

    curl_multi_remove_handle(utils->multi_handle, conn->easy_handle);
    curl_multi_add_handle(utils->multi_handle, conn->easy_handle);
  }
  else {
    /* @todo this will break when performing synchronous transfers */
    uv_timer_start(&conn->p_bucket->timer, &_uv_queue_resume_cb, retry_after*1000, 0);
    _concord_queue_pause(&conn->p_bucket->queue);
  }

  safe_free(conn->response_body.str);
  conn->response_body.size = 0;
}

static void
_concord_asynchronous_perform(concord_utils_st *utils, CURL *easy_handle)
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

  DEBUG_PRINT("Conn URL: %s", url);

  switch (http_code){
  case DISCORD_OK:
      _concord_200_async_action(utils, conn);
      return;
  case DISCORD_TOO_MANY_REQUESTS:
      _concord_429_async_action(utils, conn);
      return;
  case CURL_NO_RESPONSE: 
      DEBUG_ASSERT(!url || !*url, "No server response has been received");
      curl_multi_remove_handle(utils->multi_handle, easy_handle);
      return;
  default:
      DEBUG_PRINT("Found not yet implemented HTTP Code: %d", http_code);
      abort();
  }
}

static void
_concord_tryperform_asynchronous(concord_utils_st *utils)
{
  /* These are related to the current easy_handle being read */
  CURLMsg *msg; /* for picking up messages with the transfer status */
  int pending; /*how many messages are left */

  /* search for completed easy_handle transfers, and perform the
      instructions given by the transfer response */
  while ( (msg = curl_multi_info_read(utils->multi_handle, &pending)) )
  {
    if (CURLMSG_DONE != msg->msg)
      continue;
    
    DEBUG_PRINT("Transfers Running: %d\n\tTransfers On Hold: %d", utils->transfers_running, utils->transfers_onhold);
    
    _concord_asynchronous_perform(utils, msg->easy_handle);
  }
}

static void
_uv_perform_cb(uv_poll_t *req, int status, int events)
{
  concord_utils_st *utils = req->data;
  struct concord_context_s *context = (struct concord_context_s*)req;

  int flags = 0;
  if (events & UV_READABLE) flags |= CURL_CSELECT_IN;
  if (events & UV_WRITABLE) flags |= CURL_CSELECT_OUT;

  CURLMcode mcode = curl_multi_socket_action(utils->multi_handle, context->sockfd, flags, &utils->transfers_running);
  DEBUG_ASSERT(CURLM_OK == mcode, curl_multi_strerror(mcode));

  _concord_tryperform_asynchronous(utils);

  (void)status;
}

static void
_uv_on_timeout_cb(uv_timer_t *req)
{
  concord_utils_st *utils = req->data;

  CURLMcode mcode = curl_multi_socket_action(utils->multi_handle, CURL_SOCKET_TIMEOUT, 0, &utils->transfers_running);
  DEBUG_ASSERT(CURLM_OK == mcode, curl_multi_strerror(mcode));

  _concord_tryperform_asynchronous(utils);
}

int
Curl_start_timeout_cb(CURLM *multi_handle, long timeout_ms, void *p_userdata)
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
Curl_handle_socket_cb(CURL *easy_handle, curl_socket_t sockfd, int action, void *p_userdata, void *p_socket)
{
  concord_utils_st *utils = p_userdata;
  CURLMcode mcode;
  int uvcode;
  int events = 0;
  struct concord_context_s *context;


  switch (action){
  case CURL_POLL_IN:
  case CURL_POLL_OUT:
      if (p_socket){
        context = (struct concord_context_s*)p_socket;
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
      if (p_socket){
        uvcode = uv_poll_stop(&((struct concord_context_s*)p_socket)->poll_handle);
        DEBUG_ASSERT(!uvcode, uv_strerror(uvcode));

        _concord_context_destroy((struct concord_context_s*)p_socket);

        mcode = curl_multi_assign(utils->multi_handle, sockfd, NULL);
        DEBUG_ASSERT(CURLM_OK == mcode, curl_multi_strerror(mcode));
      }
      break;
  default:
      DEBUG_PRINT("Unknown CURL_POLL_XXX option encountered\n\tCode: %d", action);
      abort();
  }

  (void)easy_handle;

  return 0;
}

void
concord_dispatch(concord_st *concord)
{
  concord_utils_st *utils = concord->utils;

  if (!utils->transfers_onhold){
    DEBUG_PUTS("No transfers on hold, returning ..."); 
    return;
  }

  Concord_start_client_buckets(utils);

  int uvcode = uv_run(utils->loop, UV_RUN_DEFAULT);
  DEBUG_ASSERT(!uvcode, uv_strerror(uvcode));
  
  DEBUG_ASSERT(!utils->transfers_onhold, "Left loop with pending transfers");
  DEBUG_ASSERT(!utils->transfers_running, "Left loop with running transfers");

  Concord_stop_client_buckets(utils);
}

static void
_concord_200_sync_action(concord_utils_st *utils, struct concord_conn_s *conn)
{
  long long delay_ms = Concord_parse_ratelimit_header(conn->p_bucket, utils->header, true);
  uv_sleep(delay_ms);

  _concord_load_obj_perform(conn);
  conn->status = INNACTIVE;
}

/* for synchronous 429, we won't be dealing with global and non-global ratelimits
    separately, both will block all connections the same. This shouldn't affect
    performance much, as synchronous transfers are only performed when trying to
    match a first time seen bucket_key with a bucket */
static void
_concord_429_sync_action(struct concord_conn_s *conn)
{
  char message[256] = {0};
  long long retry_after;

  jscon_scanf(conn->response_body.str,
    "#message%ls " \
    "#retry_after%jd",
     message,
     &retry_after);

  DEBUG_PRINT("Being ratelimited:\t%s", message);

  uv_sleep(retry_after*1000);

  safe_free(conn->response_body.str);
  conn->response_body.size = 0;
}

void
Concord_register_bucket_key(concord_utils_st *utils, struct concord_conn_s *conn, char bucket_key[])
{
  DEBUG_PUTS("Unknown bucket key, performing connection to get bucket hash");

  enum discord_http_code http_code; /* http response code */
  char *url = NULL; /* URL from request */
  CURLcode ecode;


  ecode = curl_easy_perform(conn->easy_handle);
  DEBUG_ASSERT(CURLE_OK == ecode, curl_easy_strerror(ecode));

  ecode = curl_easy_getinfo(conn->easy_handle, CURLINFO_RESPONSE_CODE, &http_code);
  DEBUG_ASSERT(CURLE_OK == ecode, curl_easy_strerror(ecode));

  ecode = curl_easy_getinfo(conn->easy_handle, CURLINFO_EFFECTIVE_URL, &url);
  DEBUG_ASSERT(CURLE_OK == ecode, curl_easy_strerror(ecode));

  DEBUG_PRINT("Conn URL: %s", url);

  switch (http_code){
  case DISCORD_TOO_MANY_REQUESTS:
      _concord_429_sync_action(conn);

      /* Try to recover from being ratelimited */

      ecode = curl_easy_perform(conn->easy_handle);
      DEBUG_ASSERT(CURLE_OK == ecode, curl_easy_strerror(ecode));

      ecode = curl_easy_getinfo(conn->easy_handle, CURLINFO_RESPONSE_CODE, &http_code);
      DEBUG_ASSERT(CURLE_OK == ecode, curl_easy_strerror(ecode));

      DEBUG_ASSERT(DISCORD_OK == http_code, "Couldn't recover from ratelimit");
  /* FALLTHROUGH */
  case DISCORD_OK:
      _concord_200_sync_action(utils, conn);
      break;
  case CURL_NO_RESPONSE: 
      DEBUG_ASSERT(!url || !*url, "No server response has been received");
      return; /* early exit */
  default:
      DEBUG_PRINT("Found not yet implemented HTTP Code: %d", http_code);
      abort();
  }

  /* from this point forward its assumed that the transfer
      was succesful (200 OK) */

  char *bucket_hash = dictionary_get(utils->header, "x-ratelimit-bucket");
  DEBUG_PRINT("Bucket Hash: %s", bucket_hash);

  /* create bucket if it doesn't exist, otherwise, get existing one */
  struct concord_bucket_s *bucket = Concord_trycreate_bucket(utils, bucket_hash);

  /* try to find a empty bucket queue slot */
  size_t i = bucket->queue.top_onhold;
  while (NULL != bucket->queue.conns[i]){
    ++i;
  }
  DEBUG_ASSERT(i < bucket->queue.size, "Queue has reached its threshold");

  bucket->queue.conns[i] = conn; /* append conn to empty spot found */
  conn->p_bucket = bucket; /* reference bucket from conn */

  void *res = dictionary_set(utils->bucket_dict, bucket_key, bucket, NULL);
  DEBUG_ASSERT(res == bucket, "Can't link bucket key with an existing bucket");
}
