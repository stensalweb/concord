#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#include <libconcord.h>

#include "hashtable.h"
#include "debug.h"
#include "http_private.h"
#include "ratelimit.h"
#include "dispatch.h"


/* this is a very crude http header parser, splits key/value pairs at ':' */
static size_t
_curl_header_cb(char *content, size_t size, size_t nmemb, void *p_userdata)
{
  size_t realsize = size * nmemb;
  struct dictionary_s *header = p_userdata;

  char *ptr;
  if ( NULL == (ptr = strchr(content, ':')) )
    return realsize; //couldn't find key/value pair, return

  *ptr = '\0'; /* isolate key from value at ':' */
  
  char *key = content;

  if ( NULL == (ptr = strstr(ptr+1, "\r\n")) )
    return realsize; //couldn't find CRLF

  *ptr = '\0'; /* remove CRLF from value */

  /* trim space from start of value string if necessary */
  int i=1; //start from one position after ':' char
  for ( ; isspace(content[strlen(content)+i]) ; ++i)
    continue;

  char *field = strdup(&content[strlen(content)+i]);
  debug_assert(NULL != field, "Out of memory");

  /* store key/value pair in a dictionary */
  char *ret = dictionary_set(header, key, field, &free);
  debug_assert(ret == field, "Couldn't fetch header content");

  return realsize; /* return value for curl internals */
}

/* get curl response body */
static size_t
_curl_body_cb(char *content, size_t size, size_t nmemb, void *p_userdata)
{
  size_t realsize = size * nmemb;
  struct curl_response_s *response_body = p_userdata;

  char *tmp = realloc(response_body->str, response_body->size + realsize + 1);
  debug_assert(NULL != tmp, "Out of memory");

  response_body->str = tmp;
  memcpy(response_body->str + response_body->size, content, realsize);
  response_body->size += realsize;
  response_body->str[response_body->size] = '\0';

  return realsize;
}

/* init easy handle with some default opt */
CURL*
_curl_easy_default_init(concord_utils_st *utils, struct concord_conn_s *conn)
{
  CURL *new_easy_handle = curl_easy_init();
  debug_assert(NULL != new_easy_handle, "Out of memory");

  curl_easy_setopt(new_easy_handle, CURLOPT_HTTPHEADER, utils->request_header);
  curl_easy_setopt(new_easy_handle, CURLOPT_FAILONERROR, 1L);
//  curl_easy_setopt(new_easy_handle, CURLOPT_VERBOSE, 2L);

  // SET CURL_EASY CALLBACKS //
  curl_easy_setopt(new_easy_handle, CURLOPT_WRITEFUNCTION, &_curl_body_cb);
  curl_easy_setopt(new_easy_handle, CURLOPT_WRITEDATA, &conn->response_body);

  curl_easy_setopt(new_easy_handle, CURLOPT_HEADERFUNCTION, &_curl_header_cb);
  curl_easy_setopt(new_easy_handle, CURLOPT_HEADERDATA, utils->header);

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
  debug_assert(NULL != bucket_key, "Bucket key not specified (NULL)");

  struct concord_conn_s *new_conn = safe_malloc(sizeof *new_conn);

  new_conn->easy_handle = _curl_easy_default_init(utils, new_conn);

  char easy_key[18];
  sprintf(easy_key, "%p", new_conn->easy_handle);
  dictionary_set(utils->easy_dict, easy_key, new_conn, &_concord_conn_destroy);

  return new_conn;
}

static void
_curl_set_method(struct concord_conn_s *conn, enum http_method method)
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
    debug_puts("Unknown HTTP Method");
    exit(EXIT_FAILURE);
  }
}

static void
_curl_set_url(struct concord_conn_s *conn, char endpoint[])
{
  char base_url[MAX_URL_LENGTH] = BASE_URL;
  curl_easy_setopt(conn->easy_handle, CURLOPT_URL, strcat(base_url, endpoint));
}

static void
_concord_sync_perform(concord_utils_st *utils, struct concord_conn_s *conn)
{
  CURLcode ecode = curl_easy_perform(conn->easy_handle);
  debug_assert(CURLE_OK == ecode, curl_easy_strerror(ecode));

  if (NULL != conn->response_body.str){
    int remaining = dictionary_get_strtoll(utils->header, "x-ratelimit-remaining");
    debug_print("Remaining connections: %d", remaining);
    if (0 == remaining){
      long long delay_ms = Concord_parse_ratelimit_header(utils->header, false);
      debug_print("Delay_ms: %lld", delay_ms);
      uv_sleep(delay_ms);
    }

    //debug_puts(conn->response_body.str);
    (*conn->load_cb)(conn->p_object, &conn->response_body);

    conn->p_object = NULL;

    safe_free(conn->response_body.str);
    conn->response_body.size = 0;
  }
}

static void
_concord_build_bucket(
  concord_utils_st *utils,
  void **p_object, 
  concord_load_obj_ft *load_cb,
  enum http_method http_method,
  char bucket_key[],
  char url_route[])
{
  struct concord_bucket_s *bucket = dictionary_get(utils->bucket_dict, bucket_key);
  debug_puts(bucket_key);
  
  if (NULL == bucket){
    /* because this is the first time using this bucket_key, we will perform a blocking
        connection to the Discord API, in order to link this bucket_key with a bucket hash */
    struct concord_conn_s *new_conn = _concord_conn_init(utils, bucket_key);
    debug_assert(NULL != new_conn, "Out of memory");
    debug_puts("New conn created");


    _curl_set_method(new_conn, http_method); //set the http request method (GET, POST, ...)
    _curl_set_url(new_conn, url_route); //set the http request url

    new_conn->load_cb = load_cb;
    new_conn->p_object = p_object; //save object for when load_cb is executed
    
    _concord_sync_perform(utils, new_conn); //execute a blocking connection to generate this bucket hash

    debug_puts("Fetched conn matching hashbucket");

    char *bucket_hash = dictionary_get(utils->header, "x-ratelimit-bucket");
    debug_puts(bucket_hash);

    bucket = Concord_get_hashbucket(utils, bucket_hash); //return created/found bucket matching bucket hash

    /* add inactive connection to first empty slot encountered */
    size_t i = bucket->top;
    while (NULL != bucket->queue[i]){
      ++i;
    }
    debug_assert(i < bucket->num_conn, "Queue top has reached threshold");
    bucket->queue[i] = new_conn;
    new_conn->p_bucket = bucket;

    dictionary_set(utils->bucket_dict, bucket_key, bucket, NULL); //link this bucket_key with created/found hashbucket
  }
  else {
    /* add connection to bucket or reuse innactive existing one */
    debug_print("Matching hashbucket found: %s", bucket->hash_key);

    debug_assert(bucket->top < bucket->num_conn, "Queue top has reached threshold");

    struct concord_conn_s *new_conn = bucket->queue[bucket->top];
    if (NULL == new_conn){
      debug_puts("Bucket exists but needs a new connection pushed to it (can't recycle)");

      new_conn = _concord_conn_init(utils, bucket_key);
      debug_assert(NULL != new_conn, "Out of memory");

      Concord_queue_push(utils, bucket, new_conn);
    } else { 
      debug_puts("Recycling existing connection");
      Concord_queue_recycle(utils, bucket);
    }

    _curl_set_method(new_conn, http_method);
    _curl_set_url(new_conn, url_route);

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
  char *bucket_key = Concord_tryget_major(endpoint);
  debug_puts(bucket_key);

  _concord_build_bucket(
             utils,
             p_object,
             load_cb,
             http_method,
             bucket_key,
             url_route);
}

/* @todo create distinction between bot and user token */
static struct curl_slist*
_curl_init_request_header(concord_utils_st *utils)
{
  char auth[MAX_HEADER_LENGTH] = "Authorization: Bot "; 

  struct curl_slist *new_header = NULL;
  new_header = curl_slist_append(new_header,"X-RateLimit-Precision: millisecond");
  debug_assert(NULL != new_header, "Couldn't create request header");

  new_header = curl_slist_append(new_header, strcat(auth, utils->token));
  debug_assert(NULL != new_header, "Couldn't create request header");

  new_header = curl_slist_append(new_header,"User-Agent: concord (http://github.com/LucasMull/concord, v0.0)");
  debug_assert(NULL != new_header, "Couldn't create request header");

  new_header = curl_slist_append(new_header,"Content-Type: application/json");
  debug_assert(NULL != new_header, "Couldn't create request header");

  return new_header;
}

static void
_concord_utils_init(char token[], concord_utils_st *new_utils)
{
  new_utils->loop = uv_default_loop();

  new_utils->token = strndup(token, strlen(token)-1);
  debug_assert(NULL != new_utils->token, "Out of memory");

  new_utils->request_header = _curl_init_request_header(new_utils);

  uv_timer_init(new_utils->loop, &new_utils->timeout);
  new_utils->timeout.data = new_utils;

  new_utils->multi_handle = curl_multi_init();
  curl_multi_setopt(new_utils->multi_handle, CURLMOPT_SOCKETFUNCTION, &Curl_handle_socket_cb);
  curl_multi_setopt(new_utils->multi_handle, CURLMOPT_SOCKETDATA, new_utils);
  curl_multi_setopt(new_utils->multi_handle, CURLMOPT_TIMERFUNCTION, &Curl_start_timeout_cb);
  curl_multi_setopt(new_utils->multi_handle, CURLMOPT_TIMERDATA, &new_utils->timeout);

  new_utils->bucket_dict = dictionary_init();
  dictionary_build(new_utils->bucket_dict, UTILS_HASHTABLE_SIZE);

  new_utils->easy_dict = dictionary_init();
  dictionary_build(new_utils->easy_dict, UTILS_HASHTABLE_SIZE);

  new_utils->header = dictionary_init();
  dictionary_build(new_utils->header, 15);
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
  if (UV_EBUSY == uvcode){
    uv_walk(utils->loop, &_uv_on_walk_cb, NULL);

    uvcode = uv_run(utils->loop, UV_RUN_DEFAULT);
    debug_assert(!uvcode, uv_strerror(uvcode));

    uvcode = uv_loop_close(utils->loop);
    debug_assert(!uvcode, uv_strerror(uvcode));
  }
  
  curl_slist_free_all(utils->request_header);
  curl_multi_cleanup(utils->multi_handle);

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
  debug_assert(0 == code, "Couldn't start curl_global_init()");
}

void
concord_global_cleanup(){
  curl_global_cleanup();
}
