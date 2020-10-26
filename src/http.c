#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#include <libconcord.h>

#include "hashtable.h"
#include "logger.h"
#include "http_private.h"
#include "utils.h"

/* this is a very crude http header parser, it splitskey/value pairs 
    at ':' char */
/* @todo replace \r\n with a \0 before passing the value to dict */
static size_t
_concord_curl_header_cb(char *content, size_t size, size_t nmemb, void *p_userdata)
{
  size_t realsize = size * nmemb;
  struct dictionary_s *header = (struct dictionary_s*)p_userdata;

  /* splits key from value at the current header line being read */
  int len=0;
  while (!iscntrl(content[len])) /* stops at CRLF */
  {
    if (':' != content[len]){
      /* these chars belong to the key */
      ++len;
      continue;
    } 
    content[len] = '\0'; /* isolate key from value at ':' */
    /* equivalent to "key\0value\0" */

    /* len+2 to skip ':' between key and value */
    char *field = strndup(&content[len+2], realsize - len+2);
    logger_assert(NULL != field, "Out of memory");

    /* update field to dictionary */
    void *ret = dictionary_set(header, content, field, &free);
    logger_assert(NULL != ret, "Couldn't fetch header content");
    
    break;
  }

  return realsize; /* return value for curl internals */
}

/* get curl response body */
static size_t
_concord_curl_body_cb(char *content, size_t size, size_t nmemb, void *p_userdata)
{
  size_t realsize = size * nmemb;
  struct curl_response_s *response_body = (struct curl_response_s*)p_userdata;

  char *tmp = realloc(response_body->str, response_body->size + realsize + 1);
  logger_assert(NULL != tmp, "Out of memory");

  response_body->str = tmp;
  memcpy(response_body->str + response_body->size, content, realsize);
  response_body->size += realsize;
  response_body->str[response_body->size] = '\0';

  return realsize;
}

/* init easy handle with some default opt */
CURL*
_concord_curl_easy_init(concord_utils_st *utils, struct concord_conn_s *conn)
{
  CURL *new_easy_handle = curl_easy_init();
  logger_assert(NULL != new_easy_handle, "Out of memory");

  curl_easy_setopt(new_easy_handle, CURLOPT_HTTPHEADER, utils->request_header);
  curl_easy_setopt(new_easy_handle, CURLOPT_FAILONERROR, 1L);
//  curl_easy_setopt(new_easy_handle, CURLOPT_VERBOSE, 2L);

  // SET CURL_EASY CALLBACKS //
  curl_easy_setopt(new_easy_handle, CURLOPT_WRITEFUNCTION, &_concord_curl_body_cb);
  curl_easy_setopt(new_easy_handle, CURLOPT_WRITEDATA, &conn->response_body);

  curl_easy_setopt(new_easy_handle, CURLOPT_HEADERFUNCTION, &_concord_curl_header_cb);
  curl_easy_setopt(new_easy_handle, CURLOPT_HEADERDATA, utils->header); /* @todo change to bucket->header */

  return new_easy_handle;
}

/* @param ptr is NULL because we want to pass this function as a
    destructor callback to dictionary_set, which only accepts
    destructors with void* param */
static void
_concord_conn_destroy(void *ptr)
{
  struct concord_conn_s *conn = ptr;

  curl_easy_cleanup(conn->easy_handle);
  safe_free(conn->response_body.str);
  safe_free(conn);
}


/* appends new connection node to the end of the list */
static struct concord_conn_s*
_concord_conn_init(concord_utils_st *utils, char bucket_key[])
{
  logger_assert(NULL != bucket_key, "Bucket key not specified (NULL)");

  struct concord_conn_s *new_conn = safe_malloc(sizeof *new_conn);

  new_conn->easy_handle = _concord_curl_easy_init(utils, new_conn);

  char easy_key[18];
  sprintf(easy_key, "%p", new_conn->easy_handle);
  dictionary_set(utils->easy_dict, easy_key, new_conn, &_concord_conn_destroy);

  return new_conn;
}

static void
_http_set_method(struct concord_conn_s *conn, enum http_method method)
{
  switch (method){
  case DELETE:
      curl_easy_setopt(conn->easy_handle, CURLOPT_CUSTOMREQUEST, "DELETE");
      return;
  case GET:
      curl_easy_setopt(conn->easy_handle, CURLOPT_HTTPGET, 1L);
      return;
  case POST:
      curl_easy_setopt(conn->easy_handle, CURLOPT_POST, 1L);
      return;
  case PATCH:
      curl_easy_setopt(conn->easy_handle, CURLOPT_CUSTOMREQUEST, "PATCH");
      return;
  case PUT:
      curl_easy_setopt(conn->easy_handle, CURLOPT_UPLOAD, 1L);
      return;
  default:
    logger_puts("Unknown HTTP Method");
    exit(EXIT_FAILURE);
  }
}

static void
_http_set_url(struct concord_conn_s *conn, char endpoint[])
{
  char base_url[MAX_URL_LENGTH] = BASE_URL;
  curl_easy_setopt(conn->easy_handle, CURLOPT_URL, strcat(base_url, endpoint));
}

static void
_concord_sync_perform(concord_utils_st *utils, struct concord_conn_s *conn)
{
  CURLcode ecode = curl_easy_perform(conn->easy_handle);
  logger_assert(CURLE_OK == ecode, curl_easy_strerror(ecode));

  if (NULL != conn->response_body.str){
    int remaining = dictionary_get_strtoll(utils->header, "x-ratelimit-remaining");
    logger_print("Remaining connections: %d", remaining);
    if (0 == remaining){
      long long delay_ms = Utils_parse_ratelimit_header(utils->header, false);
      logger_print("Delay_ms: %lld", delay_ms);
      uv_sleep(delay_ms);
    }

    //logger_puts(conn->response_body.str);
    (*conn->load_cb)(conn->p_object, &conn->response_body);

    conn->p_object = NULL;

    safe_free(conn->response_body.str);
    conn->response_body.size = 0;
  }
}

static struct concord_bucket_s*
_concord_bucket_init(char bucket_hash[])
{
  struct concord_bucket_s *new_bucket = safe_malloc(sizeof *new_bucket);

  new_bucket->num_conn = MAX_CONCURRENT_CONNS;
  new_bucket->queue = safe_malloc(sizeof *new_bucket->queue * new_bucket->num_conn);

  new_bucket->hash_key = strndup(bucket_hash, strlen(bucket_hash));
  logger_assert(NULL != new_bucket->hash_key, "Out of memory");

  return new_bucket;
}

/* @param ptr is NULL because we want to pass this function as a
    destructor callback to dictionary_set, which only accepts
    destructors with void* param */
static void
_concord_bucket_destroy(void *ptr)
{
  struct concord_bucket_s *bucket = ptr;

  safe_free(bucket->queue);

  safe_free(bucket->hash_key);
  safe_free(bucket);
}

static void
_concord_queue_recycle(concord_utils_st *utils, struct concord_bucket_s *bucket)
{
  logger_assert(NULL != bucket->queue[bucket->top], "Can't recycle empty slot");
  logger_assert(bucket->top < bucket->num_conn, "Queue top has reached threshold");

  ++bucket->top;
  ++utils->active_handles;

  logger_print("Bucket top is: %ld", bucket->top);
  logger_print("Bucket size is: %ld", bucket->num_conn);

  if (MAX_CONCURRENT_CONNS == utils->active_handles){
    logger_puts("Reach max concurrent connections threshold, auto performing connections on hold ...");
    concord_dispatch((concord_st*)utils);
  }
}

/* push new connection to queue */
static void
_concord_queue_push(concord_utils_st *utils, struct concord_bucket_s *bucket, struct concord_conn_s *conn)
{
  logger_assert(bucket->top < bucket->num_conn, "Queue top has reached threshold");

  bucket->queue[bucket->top] = conn; 
  conn->bucket = bucket;

  ++bucket->top;
  ++utils->active_handles;

  logger_print("Bucket top is: %ld", bucket->top);
  logger_print("Bucket size is: %ld", bucket->num_conn);

  if (MAX_CONCURRENT_CONNS == utils->active_handles){
    logger_puts("Reach max concurrent connections threshold, auto performing connections on hold ...");
    concord_dispatch((concord_st*)utils);
  }
}

static void
_concord_queue_pop(concord_utils_st *utils, struct concord_bucket_s *bucket)
{
  if (bucket->bottom == bucket->top) return; //nothing to pop

  struct concord_conn_s *conn = bucket->queue[bucket->bottom];
  logger_assert(NULL != conn, "Can't pop empty queue's slot");

  curl_multi_add_handle(utils->multi_handle, conn->easy_handle);
  logger_print("Queue Slot: %ld", bucket->bottom);

  ++bucket->bottom;
  --utils->active_handles;

  logger_print("Bucket top is: %ld", bucket->top);
  logger_print("Bucket size is: %ld", bucket->num_conn);
}

static void
_concord_client_buckets_append(concord_utils_st *utils, struct concord_bucket_s *bucket)
{
  ++utils->num_buckets;
  void *tmp = realloc(utils->client_buckets, sizeof *utils->client_buckets * utils->num_buckets);
  logger_assert(NULL != tmp, "Out of memory");

  utils->client_buckets = tmp;

  utils->client_buckets[utils->num_buckets-1] = bucket;
}

static void
_concord_start_client_buckets(concord_utils_st *utils)
{
  for (size_t i=0; i < utils->num_buckets; ++i){
    _concord_queue_pop(utils, utils->client_buckets[i]);
    logger_print("Bucket Hash: %s\n\tBucket Size: %ld", utils->client_buckets[i]->hash_key, utils->client_buckets[i]->top);
  }
}

static void
_concord_reset_client_buckets(concord_utils_st *utils)
{
  for (size_t i=0; i < utils->num_buckets; ++i){
    utils->client_buckets[i]->top = 0;
    utils->client_buckets[i]->bottom = 0;
  }
}

static struct concord_bucket_s*
_concord_get_hashbucket(concord_utils_st *utils, char bucket_hash[])
{
  logger_assert(NULL != bucket_hash, "Bucket hash unspecified (NULL)");

  /* check if hashbucket with bucket_hash already exists */
  struct concord_bucket_s *bucket = dictionary_get(utils->bucket_dict, bucket_hash);

  if (NULL != bucket){
    logger_puts("returning existing bucket");
    return bucket; //bucket exists return it
  }

  /* hashbucket doesn't exist, create it */
  bucket = _concord_bucket_init(bucket_hash);

  dictionary_set(utils->bucket_dict, bucket_hash, bucket, &_concord_bucket_destroy);

  _concord_client_buckets_append(utils, bucket);

  logger_puts("returning new bucket");
  return bucket;
}

static void
_concord_start_conn(
  concord_utils_st *utils,
  void **p_object, 
  concord_load_obj_ft *load_cb,
  enum http_method http_method,
  char bucket_key[],
  char url_route[])
{
  struct concord_bucket_s *bucket = dictionary_get(utils->bucket_dict, bucket_key);
  logger_puts(bucket_key);
  
  if (NULL == bucket){
    /* because this is the first time using this bucket_key, we will perform a blocking
        connection to the Discord API, in order to link this bucket_key with a bucket hash */
    struct concord_conn_s *new_conn = _concord_conn_init(utils, bucket_key);
    logger_assert(NULL != new_conn, "Out of memory");
    logger_puts("new conn created");


    _http_set_method(new_conn, http_method); //set the http request method (GET, POST, ...)
    _http_set_url(new_conn, url_route); //set the http request url

    new_conn->load_cb = load_cb;
    new_conn->p_object = p_object; //save object for when load_cb is executed
    
    _concord_sync_perform(utils, new_conn); //execute a blocking connection to generate this bucket hash

    logger_puts("fetched conn matching hashbucket");

    char *bucket_hash = dictionary_get(utils->header, "x-ratelimit-bucket");
    logger_puts(bucket_hash);

    bucket = _concord_get_hashbucket(utils, bucket_hash); //return created/found bucket matching bucket hash

    /* add inactive connection to first empty slot encountered */
    size_t i = bucket->top;
    while (NULL != bucket->queue[i]){
      ++i;
    }
    logger_assert(i < bucket->num_conn, "Queue top has reached threshold");
    bucket->queue[i] = new_conn;
    new_conn->bucket = bucket;

    dictionary_set(utils->bucket_dict, bucket_key, bucket, NULL); //link this bucket_key with created/found hashbucket
  }
  else {
    /* add connection to bucket or reuse innactive existing one */
    logger_print("Matching hashbucket found: %s", bucket->hash_key);

    logger_assert(bucket->top < bucket->num_conn, "Queue top has reached threshold");

    struct concord_conn_s *new_conn = bucket->queue[bucket->top];
    if (NULL == new_conn){
      logger_puts("bucket exists but needs a new connection pushed to it (not recycling)");

      new_conn = _concord_conn_init(utils, bucket_key);
      logger_assert(NULL != new_conn, "Out of memory");

      _concord_queue_push(utils, bucket, new_conn);
    } else { 
      logger_puts("recycling existing connection");
      _concord_queue_recycle(utils, bucket);
    }

    _http_set_method(new_conn, http_method);
    _http_set_url(new_conn, url_route);

    new_conn->load_cb = load_cb;
    new_conn->p_object = p_object;
  }
}

void
Concord_http_request(
  concord_utils_st *utils, 
  void **p_object, 
  concord_load_obj_ft *load_cb,
  enum http_method http_method,
  char endpoint[],
  ...)
{
  /* create url_route */
  va_list args;
  va_start (args, endpoint);

  char url_route[ENDPOINT_LENGTH];
  vsprintf(url_route, endpoint, args);

  va_end(args);

  /* try to get major parameter for bucket key, if doesn't
      exists then will return the endpoint instead */
  char *bucket_key = Utils_tryget_major(endpoint);
  logger_puts(bucket_key);

  _concord_start_conn(
             utils,
             p_object,
             load_cb,
             http_method,
             bucket_key,
             url_route);
}

static void
_curl_check_multi_info(concord_utils_st *utils)
{
  /* See how the transfers went */
  CURLMsg *msg; /* for picking up messages with the transfer status */
  int pending; /*how many messages are left */
  long http_code; /* http response code */
  while ((msg = curl_multi_info_read(utils->multi_handle, &pending)))
  {
    if (CURLMSG_DONE != msg->msg)
      continue;

    /* Find out which handle this message is about */
    char easy_key[18];
    sprintf(easy_key, "%p", msg->easy_handle);
    struct concord_conn_s *conn = dictionary_get(utils->easy_dict, easy_key);

    curl_easy_getinfo(conn->easy_handle, CURLINFO_RESPONSE_CODE, &http_code);
    logger_assert(429 != http_code, "GLOBAL TIMEOUT RECEIVED");

    /* execute load callback to perform change in object */
    if (NULL != conn->response_body.str){
      //logger_puts(conn->response_body.str);
      (*conn->load_cb)(conn->p_object, &conn->response_body);

      conn->p_object = NULL;

      safe_free(conn->response_body.str);
      conn->response_body.size = 0;
    }

    curl_multi_remove_handle(utils->multi_handle, conn->easy_handle);

    int remaining = dictionary_get_strtoll(utils->header, "x-ratelimit-remaining");
    logger_print("Remaining connections: %d", remaining);
    if (0 == remaining){
      long long delay_ms = Utils_parse_ratelimit_header(utils->header, true);
      logger_print("Delay_ms: %lld", delay_ms);
      /* @todo sleep is blocking, we don't want this */
      uv_sleep(delay_ms);
    }

    do {
      _concord_queue_pop(utils, conn->bucket);
    } while (remaining--);
  }
}

/* wrapper around curl_multi_perform() , using poll() */
/* @todo I think I'm not using curl_multi_wait timeout parameter as I should, or it's not doing what I think it is */
void
concord_dispatch(concord_st *concord)
{
  concord_utils_st *utils = &concord->utils;

  _concord_start_client_buckets(utils);

  int transfers_running = 0; /* keep number of running handles */
  int repeats = 0;
  do {
    CURLMcode mcode;
    int numfds;

    mcode = curl_multi_perform(utils->multi_handle, &transfers_running);
    if (CURLM_OK == mcode){
      /* wait for activity, timeout or "nothing" */
      mcode = curl_multi_wait(utils->multi_handle, NULL, 0, 500, &numfds);
      _curl_check_multi_info(utils);
      logger_print("Transfers Running: %d", transfers_running);
    }

    logger_assert(CURLM_OK == mcode, curl_easy_strerror(mcode));

    /* numfds being zero means either a timeout or no file descriptor to
        wait for. Try timeout on first occurrences, then assume no file
        descriptors and no file descriptors to wait for mean wait for
        100 milliseconds. */
    if (0 == numfds){
      ++repeats; /* count number of repeated zero numfds */
      if (repeats > 1){
        uv_sleep(100); /* sleep 100 milliseconds */
      }
    } else {
      repeats = 0;
    }
  } while (utils->active_handles || transfers_running);

  _concord_reset_client_buckets(utils);

  logger_print("Active Handles: %ld", utils->active_handles);
  logger_assert(0 == utils->active_handles, "There are still active handles waiting for curl_multi_perform()");
}

void
concord_request_method(concord_st *concord, concord_request_method_et method)
{
  switch (method){
  case ASYNC_IO:
  case SYNC_IO:
      break;
  default:
      logger_puts("Undefined request method");
      exit(EXIT_FAILURE);
  }

  concord->utils.method = method;
}

/* @todo create distinction between bot and user token */
static struct curl_slist*
_curl_init_request_header(concord_utils_st *utils)
{
  char auth[MAX_HEADER_LENGTH] = "Authorization: Bot "; 

  struct curl_slist *new_header = NULL;
  new_header = curl_slist_append(new_header,"X-RateLimit-Precision: millisecond");
  logger_assert(NULL != new_header, "Couldn't create request header");

  new_header = curl_slist_append(new_header, strcat(auth, utils->token));
  logger_assert(NULL != new_header, "Couldn't create request header");

  new_header = curl_slist_append(new_header,"User-Agent: concord (http://github.com/LucasMull/concord, v0.0)");
  logger_assert(NULL != new_header, "Couldn't create request header");

  new_header = curl_slist_append(new_header,"Content-Type: application/json");
  logger_assert(NULL != new_header, "Couldn't create request header");

  return new_header;
}

static void
_concord_utils_init(char token[], concord_utils_st *new_utils)
{
  new_utils->token = strndup(token, strlen(token)-1);
  logger_assert(NULL != new_utils->token, "Out of memory");

  new_utils->request_header = _curl_init_request_header(new_utils);

  new_utils->multi_handle = curl_multi_init();

  /* @todo i need to benchmark this to see if there's actual benefit */
  new_utils->easy_share = curl_share_init();
  curl_share_setopt(new_utils->easy_share, CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE);
  curl_share_setopt(new_utils->easy_share, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
  curl_share_setopt(new_utils->easy_share, CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT);

  new_utils->bucket_dict = dictionary_init();
  dictionary_build(new_utils->bucket_dict, UTILS_HASHTABLE_SIZE);

  new_utils->easy_dict = dictionary_init();
  dictionary_build(new_utils->easy_dict, UTILS_HASHTABLE_SIZE);

  new_utils->header = dictionary_init();
  dictionary_build(new_utils->header, 15);

  /* defaults to synchronous transfers method */
  new_utils->method = SYNC_IO;
}

static void
_concord_utils_destroy(concord_utils_st *utils)
{
  curl_slist_free_all(utils->request_header);
  curl_multi_cleanup(utils->multi_handle);
  curl_share_cleanup(utils->easy_share);

  dictionary_destroy(utils->bucket_dict);
  dictionary_destroy(utils->easy_dict);

  dictionary_destroy(utils->header);

  safe_free(utils->client_buckets);

  safe_free(utils->token);
}

concord_st*
concord_init(char token[])
{
  concord_st *new_concord = safe_malloc(sizeof *new_concord);

  _concord_utils_init(token, &new_concord->utils);

  new_concord->channel = concord_channel_init(&new_concord->utils);
  new_concord->guild = concord_guild_init(&new_concord->utils);
  new_concord->user = concord_user_init(&new_concord->utils);
  new_concord->client = concord_user_init(&new_concord->utils);

  return new_concord;
}

void
concord_cleanup(concord_st *concord)
{
  _concord_utils_destroy(&concord->utils);

  concord_channel_destroy(concord->channel);
  concord_guild_destroy(concord->guild);
  concord_user_destroy(concord->user);
  concord_user_destroy(concord->client);

  safe_free(concord);
}

void
concord_global_init(){
  int code = curl_global_init(CURL_GLOBAL_DEFAULT);
  logger_assert(0 == code, "Couldn't start curl_global_init()");
}

void
concord_global_cleanup(){
  curl_global_cleanup();
}
