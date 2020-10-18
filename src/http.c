#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>

//#include <curl/curl.h>
//#include <libjscon.h>

#include <libconcord.h>

#include "http_private.h"

#include "hashtable.h"
#include "logger.h"
#include "utils_private.h"


/* @todo there has to be things I'm missing */
static size_t
_concord_curl_header_cb(char *content, size_t size, size_t nmemb, void *p_userdata)
{
  int realsize = size * nmemb;
  struct concord_header_s *header = (struct concord_header_s*)p_userdata;

  /* splits key from value at the current header line being read */
  int len=0;
  while (!iscntrl(content[len])) //stops at CRLF
  {
    if (':' != content[len]){
      /* these chars belong to the key */
      ++len;
      continue;
    } 

    /* we only want key/value pairs for ratelimiting */
    if (0 != strncmp(content, "x-ratelimit", 11)) break;

    char key[30];
    strncpy(key, content, len); //isolate key found
    key[len] = '\0';

    /* if there's a value already assigned to this key, free it and
        update its value with the new one*/
    char **rl_field = hashtable_get(header->ht, key);
    safe_free(*rl_field);
    
    //len+2 to skip ':' between key and value
    *rl_field = strndup(&content[len+2], realsize - len+2);
    assert(NULL != *rl_field);
    /*
    if (0 == strcmp(key, "x-ratelimit-bucket")){
      fprintf(stderr, "%s:%s\n", key, *rl_field);
    }
    */
    break;
  }

  //fwrite(content, 1, realsize, stderr);


  return (size_t)realsize; //return value for curl internals
}

/* get curl response body */
static size_t
_concord_curl_body_cb(char *content, size_t size, size_t nmemb, void *p_userdata)
{
  size_t realsize = size * nmemb;
  struct curl_response_s *response_body = (struct curl_response_s*)p_userdata;

  char *tmp = realloc(response_body->str, response_body->size + realsize + 1);
  assert(NULL != tmp);

  response_body->str = tmp;
  memcpy((char*)response_body->str + response_body->size, content, realsize);
  response_body->size += realsize;
  response_body->str[response_body->size] = '\0';

  return realsize;
}

/* init easy handle with some default opt */
CURL*
_concord_curl_easy_init(concord_utils_st *utils, struct concord_clist_s *conn)
{
  CURL *new_easy_handle = curl_easy_init();
  assert(NULL != new_easy_handle);

  curl_easy_setopt(new_easy_handle, CURLOPT_HTTPHEADER, utils->request_header);
  curl_easy_setopt(new_easy_handle, CURLOPT_FAILONERROR, 1L);
  //curl_easy_setopt(new_easy_handle, CURLOPT_VERBOSE, 1L);

  // SET CURL_EASY CALLBACKS //
  curl_easy_setopt(new_easy_handle, CURLOPT_WRITEFUNCTION, &_concord_curl_body_cb);
  curl_easy_setopt(new_easy_handle, CURLOPT_WRITEDATA, &conn->response_body);

  curl_easy_setopt(new_easy_handle, CURLOPT_HEADERFUNCTION, &_concord_curl_header_cb);
  curl_easy_setopt(new_easy_handle, CURLOPT_HEADERDATA, utils->header);

  return new_easy_handle;
}

/* @todo as the number of active easy_handle increases, this function becomes unnecessarily
    slow (i think? didn't really check but seems like it would), this can be solved by storing
    the last node at all times */
static struct concord_clist_s*
_concord_clist_get_last(struct concord_clist_s *conn_list)
{
  if (!conn_list) return NULL;

  struct concord_clist_s *iter = conn_list;
  while (NULL != iter->next){
    iter = iter->next;
  }

  return iter;
}

/* appends new connection node to the end of the list */
static void
_concord_clist_append(concord_utils_st *utils, struct concord_clist_s **p_new_conn)
{
  struct concord_clist_s *last;
  struct concord_clist_s *new_conn = safe_malloc(sizeof *new_conn);

  new_conn->easy_handle = _concord_curl_easy_init(utils, new_conn);

  if (NULL != p_new_conn){
    *p_new_conn = new_conn;
  }

  if (NULL == utils->conn_list){
    utils->conn_list = new_conn;
    return;
  }

  /* @todo probably better to implement a circular list */
  last = _concord_clist_get_last(utils->conn_list);
  last->next = new_conn;
}

void
_concord_clist_free_all(struct concord_clist_s *conn)
{
  if (!conn) return;

  struct concord_clist_s *next_conn;
  do {
    next_conn = conn->next;
    curl_easy_cleanup(conn->easy_handle);
    safe_free(conn->conn_key);
    safe_free(conn->easy_key);
    safe_free(conn);
    conn = next_conn;
  } while (next_conn);
}

static void
_http_set_method(struct concord_clist_s *conn, enum http_method method)
{
  /* @todo to implement commented out ones */
  switch (method){
  //case DELETE:
  case GET:
      curl_easy_setopt(conn->easy_handle, CURLOPT_HTTPGET, 1L);
      return;
  case POST:
      curl_easy_setopt(conn->easy_handle, CURLOPT_POST, 1L);
      return;
  //case PATCH:
  //case PUT:
  default:
    logger_throw("ERROR: Unknown HTTP Method");
    exit(EXIT_FAILURE);
  }
}

static void
_http_set_url(struct concord_clist_s *conn, char endpoint[])
{
  char base_url[MAX_URL_LENGTH] = BASE_URL;
  curl_easy_setopt(conn->easy_handle, CURLOPT_URL, strcat(base_url, endpoint));
}

static void
_concord_set_curl_easy(concord_utils_st *utils, struct concord_clist_s *conn)
{
  CURLcode ecode = curl_easy_perform(conn->easy_handle);
  logger_excep(CURLE_OK != ecode, curl_easy_strerror(ecode));

  if (NULL != conn->response_body.str){
    /* @todo for some reason only getting a single header when
        doing blocking, find out why */
    long long delay_ms;
    if (0 != strtol(utils->header->remaining, NULL, 10)){
      delay_ms = Utils_parse_ratelimit_header(utils->header, false);
    } else {
      delay_ms = 100;
    }

    uv_sleep(delay_ms);

    //logger_throw(conn->response_body.str);
    (*conn->load_cb)(conn->p_object, &conn->response_body);

    conn->p_object = NULL;

    safe_free(conn->response_body.str);
    conn->response_body.size = 0;
  }
}

static void
_concord_http_syncio(
  concord_utils_st *utils,
  void **p_object, 
  char conn_key[],
  char endpoint[],
  concord_ld_object_ft *load_cb,
  enum http_method http_method)
{
  struct concord_clist_s *conn = hashtable_get(utils->syncio_ht, conn_key);
  if (NULL == conn){
    /* didn't find connection node, create a new one and return it */
    _concord_clist_append(utils, &conn);
    assert(NULL != conn);

    _http_set_method(conn, http_method);

    /* share resources between SYNC_IO type easy_handles */
    curl_easy_setopt(conn->easy_handle, CURLOPT_SHARE, utils->easy_share);

    conn->load_cb = load_cb;

    conn->conn_key = strdup(conn_key);
    assert(NULL != conn->conn_key);

    /* this stores the connection node inside a hashtable
        where entries keys are the requests within each
        function
      this allows for easy_handles reusability */
    hashtable_set(utils->syncio_ht, conn->conn_key, conn);

    /* this stores connection node inside a hashtable where entries
        keys are easy handle memory address converted to string
       will be used when checking for multi_perform completed transfers */
    char easy_key[18];
    sprintf(easy_key, "%p", conn->easy_handle);
    conn->easy_key = strdup(easy_key);
    assert(NULL != conn->easy_key);

    hashtable_set(utils->easy_ht, conn->easy_key, conn);
  }

  _http_set_url(conn, endpoint);
  conn->p_object = p_object; //save object for when load_cb is executed
  
  /* if method is SYNC_IO (default), then exec current conn's easy_handle with
      easy_perform() in a blocking manner. */
  _concord_set_curl_easy(utils, conn);
}

static void
_concord_set_curl_multi(concord_utils_st *utils, struct concord_clist_s *conn)
{
  if (NULL != conn->easy_handle){
    curl_multi_add_handle(utils->multi_handle, conn->easy_handle);
    ++utils->active_handles;
  }
}

static void
_concord_http_asyncio(
  concord_utils_st *utils,
  void **p_object, 
  char conn_key[],
  char endpoint[],
  concord_ld_object_ft *load_cb,
  enum http_method http_method)
{
  struct concord_clist_s *conn = hashtable_get(utils->asyncio_ht, conn_key);
  if (NULL == conn){
    /* didn't find connection node, create a new one */
    _concord_clist_append(utils, &conn);
    assert(NULL != conn);

    conn->conn_key = strdup(conn_key);
    assert(NULL != conn->conn_key);
    
    hashtable_set(utils->asyncio_ht, conn->conn_key, conn);

    /* this stores connection node inside a hashtable where entries
        keys are easy handle memory address converted to string
       will be used when checking for multi_perform completed transfers */
    char easy_key[18];
    sprintf(easy_key, "%p", conn->easy_handle);
    conn->easy_key = strdup(easy_key);
    assert(NULL != conn->easy_key);

    hashtable_set(utils->easy_ht, conn->easy_key, conn);
  }

  _http_set_method(conn, http_method);
  _http_set_url(conn, endpoint);

  conn->load_cb = load_cb;
  conn->p_object = p_object; //save object for when load_cb is executed
  
  /* if method is ASYNC_IO, then add current's conn easy_handle to the multi stack,
      and wait until concord_dispatch() is called for asynchronous execution */
  _concord_set_curl_multi(utils, conn);
}

/* wrapper around curl_multi_perform() , using poll() */
/* @todo I think I'm not using curl_multi_wait timeout parameter as I should,
    or it's not doing what I think it is */
void
concord_dispatch(concord_st *concord)
{
  concord_utils_st *utils = concord->utils;

  int transfers_running = 0; /* keep number of running handles */
  int repeats = 0;
  long delay_ms = 100;
  do {
    CURLMcode mcode;
    int numfds;

    mcode = curl_multi_perform(utils->multi_handle, &transfers_running);
    if (delay_ms > 0 && CURLM_OK == mcode){
      /* wait for activity, timeout or "nothing" */
      mcode = curl_multi_wait(utils->multi_handle, NULL, 0, delay_ms, &numfds);
    }

    logger_excep(CURLM_OK != mcode, curl_easy_strerror(mcode));

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
      if (NULL == utils->header->remaining) continue;

      if (0 != strtol(utils->header->remaining, NULL, 10)){
        delay_ms = Utils_parse_ratelimit_header(utils->header, true);
      } else {
        delay_ms = 100;
      }

      repeats = 0;
    }
  } while (transfers_running);

  /* See how the transfers went */
  CURLMsg *msg; /* for picking up messages with the transfer status */
  int msgs_left; /*how many messages are left */
  while ((msg = curl_multi_info_read(utils->multi_handle, &msgs_left)))
  {
    if (CURLMSG_DONE != msg->msg)
      continue;

    /* Find out which handle this message is about */
    char easy_key[18];
    sprintf(easy_key, "%p", msg->easy_handle);
    struct concord_clist_s *conn = hashtable_get(utils->easy_ht, easy_key);
    assert (NULL != conn);

    /* execute load callback to perform change in object */
    if (NULL != conn->response_body.str){
      //logger_throw(conn->response_body.str);
      (*conn->load_cb)(conn->p_object, &conn->response_body);

      conn->p_object = NULL;

      safe_free(conn->response_body.str);
      conn->response_body.size = 0;
    }

    curl_multi_remove_handle(utils->multi_handle, conn->easy_handle);
    --utils->active_handles;
  }

  assert(0 == utils->active_handles);
}

void
concord_request_method(concord_st *concord, concord_request_method_et method)
{
  switch (method){
  case ASYNC_IO:
  case SYNC_IO:
      break;
  default:
      logger_throw("ERROR: undefined request method");
      exit(EXIT_FAILURE);
  }

  concord->utils->method = method;
}

/* @todo create distinction between bot and user token */
static struct curl_slist*
_concord_init_request_header(concord_utils_st *utils)
{
  char auth[MAX_HEADER_LENGTH] = "Authorization: Bot "; 

  struct curl_slist *new_header = NULL;
  new_header = curl_slist_append(new_header,"X-RateLimit-Precision: millisecond");
  assert(NULL != new_header);

  new_header = curl_slist_append(new_header, strcat(auth, utils->token));
  assert(NULL != new_header);

  new_header = curl_slist_append(new_header,"User-Agent: concord (http://github.com/LucasMull/concord, v0.0)");
  assert(NULL != new_header);

  new_header = curl_slist_append(new_header,"Content-Type: application/json");
  assert(NULL != new_header);

  return new_header;
}

#define XRL_BUCKET      "x-ratelimit-bucket"
#define XRL_LIMIT       "x-ratelimit-limit"
#define XRL_REMAINING   "x-ratelimit-remaining"
#define XRL_RESET       "x-ratelimit-reset"
#define XRL_RESET_AFTER "x-ratelimit-reset-after"

static struct concord_header_s*
_concord_utils_header_init()
{
  struct concord_header_s *new_header = safe_malloc(sizeof *new_header);

  new_header->ht = hashtable_init();
  hashtable_build(new_header->ht, 15);

  char *key; 
  key = strdup(XRL_BUCKET);
  assert(NULL != key);
  hashtable_set(new_header->ht, key, &new_header->bucket);

  key = strdup(XRL_LIMIT);
  assert(NULL != key);
  hashtable_set(new_header->ht, key, &new_header->limit);

  key = strdup(XRL_REMAINING);
  assert(NULL != key);
  hashtable_set(new_header->ht, key, &new_header->remaining);

  key = strdup(XRL_RESET);
  assert(NULL != key);
  hashtable_set(new_header->ht, key, &new_header->reset);

  key = strdup(XRL_RESET_AFTER);
  assert(NULL != key);
  hashtable_set(new_header->ht, key, &new_header->reset_after);

  return new_header;
}

static void
_concord_utils_header_destroy(struct concord_header_s *header)
{
  hashtable_destroy_dict(header->ht);

  safe_free(header);
}

static concord_utils_st*
_concord_utils_init(char token[])
{
  concord_utils_st *new_utils = safe_malloc(sizeof *new_utils + strlen(token));
  strncpy(new_utils->token, token, strlen(token)-1);

  new_utils->request_header = _concord_init_request_header(new_utils);
  new_utils->header = _concord_utils_header_init();

  new_utils->multi_handle = curl_multi_init();

  /* @todo i need to benchmark this to see if there's actual benefit */
  new_utils->easy_share = curl_share_init();
  curl_share_setopt(new_utils->easy_share, CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE);
  curl_share_setopt(new_utils->easy_share, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
  curl_share_setopt(new_utils->easy_share, CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT);

  new_utils->asyncio_ht = hashtable_init();
  hashtable_build(new_utils->asyncio_ht, UTILS_HASHTABLE_SIZE);

  new_utils->syncio_ht = hashtable_init();
  hashtable_build(new_utils->syncio_ht, UTILS_HASHTABLE_SIZE);

  new_utils->easy_ht = hashtable_init();
  hashtable_build(new_utils->easy_ht, UTILS_HASHTABLE_SIZE);

  /* defaults to synchronous transfers method */
  new_utils->method = SYNC_IO;

  return new_utils;
}

static void
_concord_utils_destroy(concord_utils_st *utils)
{
  curl_slist_free_all(utils->request_header);
  curl_multi_cleanup(utils->multi_handle);
  _concord_clist_free_all(utils->conn_list);
  curl_share_cleanup(utils->easy_share);
  hashtable_destroy(utils->asyncio_ht);
  hashtable_destroy(utils->syncio_ht);
  hashtable_destroy(utils->easy_ht);
  _concord_utils_header_destroy(utils->header);

  safe_free(utils);
}

void
Concord_http_request(
  concord_utils_st *utils, 
  void **p_object, 
  char conn_key[],
  char endpoint[], 
  concord_ld_object_ft *load_cb, 
  enum http_method http_method)
{
  switch (utils->method){
  case ASYNC_IO:
   {
      /* for asyncio we will create exclusive easy_handles, so that there won't
          be any conflict changing between SYNC_IO and ASYNC_IO methods, also, to
          guarantee that each easy_handle will have a unique identifier before
          dispatching for asynchronous requests */
      char task_key[15];
      sprintf(task_key, "AsyncioTask#%ld", utils->active_handles);

      _concord_http_asyncio(
                 utils,
                 p_object,
                 task_key,
                 endpoint,
                 load_cb,
                 http_method);
      break;
   }
  case SYNC_IO:
      _concord_http_syncio(
                 utils,
                 p_object,
                 conn_key,
                 endpoint,
                 load_cb,
                 http_method);
      break;
  default:
      logger_throw("ERROR: undefined request method");
      exit(EXIT_FAILURE);
  }
}

concord_st*
concord_init(char token[])
{
  concord_st *new_concord = safe_malloc(sizeof *new_concord);

  new_concord->utils = _concord_utils_init(token);

  new_concord->channel = concord_channel_init(new_concord->utils);
  new_concord->guild = concord_guild_init(new_concord->utils);
  new_concord->user = concord_user_init(new_concord->utils);
  new_concord->client = concord_user_init(new_concord->utils);

  return new_concord;
}

void
concord_cleanup(concord_st *concord)
{
  concord_channel_destroy(concord->channel);
  concord_guild_destroy(concord->guild);
  concord_user_destroy(concord->user);
  concord_user_destroy(concord->client);
  _concord_utils_destroy(concord->utils);

  safe_free(concord);
}

void
concord_global_init(){
  int code = curl_global_init(CURL_GLOBAL_DEFAULT);
  logger_excep(0 != code, "ERROR: Couldn't start curl_global_init()");
}

void
concord_global_cleanup(){
  curl_global_cleanup();
}
