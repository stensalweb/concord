#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

/* somewhat unix-specific */
#include <sys/time.h>
#include <unistd.h>

//#include <curl/curl.h>
//#include <libjscon.h>

#include <libconcord.h>

#include "hashtable.h"
#include "api_wrapper_private.h"
#include "logger.h"

static size_t
_concord_curl_write_cb(char *content, size_t size, size_t nmemb, void *p_userdata)
{
  size_t realsize = size * nmemb;
  struct curl_response_s *chunk = (struct curl_response_s*)p_userdata;

  char *tmp = realloc(chunk->response, chunk->size + realsize + 1);

  if (tmp == NULL) return 0;

  chunk->response = tmp;
  memcpy((char*)chunk->response + chunk->size, content, realsize);
  chunk->size += realsize;
  chunk->response[chunk->size] = '\0';

  return realsize;
}

/* init easy handle with some default opt */
CURL*
_concord_curl_easy_init(concord_utils_st *utils, struct curl_response_s *chunk)
{
  CURL *new_easy_handle = curl_easy_init();
  assert(NULL != new_easy_handle);

  curl_easy_setopt(new_easy_handle, CURLOPT_HTTPHEADER, utils->header);
  curl_easy_setopt(new_easy_handle, CURLOPT_FAILONERROR, 1L);
  curl_easy_setopt(new_easy_handle, CURLOPT_VERBOSE, 1L);

  // SET CURL_EASY CALLBACK //
  curl_easy_setopt(new_easy_handle, CURLOPT_WRITEFUNCTION, &_concord_curl_write_cb);
  curl_easy_setopt(new_easy_handle, CURLOPT_WRITEDATA, chunk);

  return new_easy_handle;
}

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

static void
_concord_clist_append(concord_utils_st *utils, struct concord_clist_s **p_new_conn, char endpoint[])
{
  struct concord_clist_s *last;
  struct concord_clist_s *new_conn = concord_malloc(sizeof *new_conn);

  new_conn->easy_handle = _concord_curl_easy_init(utils, &new_conn->chunk);

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
    concord_free(conn->conn_key);
    concord_free(conn->easy_key);
    concord_free(conn);
    conn = next_conn;
  } while (next_conn);
}

static void
_concord_perform_sync(
  concord_utils_st *utils,
  void **p_object, 
  char conn_key[],
  char endpoint[],
  concord_ld_object_ft *load_cb,
  curl_request_ft *request_cb)
{
  struct concord_clist_s *conn = hashtable_get(utils->conn_hashtable, conn_key);
  if (NULL != conn){
    /* found connection node, set new URL and return it */
    char base_url[MAX_URL_LENGTH] = BASE_URL;
    curl_easy_setopt(conn->easy_handle, CURLOPT_URL, strcat(base_url, endpoint));
  } else {
    /* didn't find connection node, create a new one and return it */
    _concord_clist_append(utils, &conn, endpoint);
    assert(NULL != conn);

    (*request_cb)(utils, conn, endpoint); //register desired request to new easy_handle

    conn->load_cb = load_cb;

    conn->conn_key = strdup(conn_key);
    assert(NULL != conn->conn_key);

    /* this stores the connection node inside a hashtable
        where entries keys are the requests within each
        function
      this allows for easy_handles reusability */
    hashtable_set(utils->conn_hashtable, conn->conn_key, conn);

    /* this stores connection node inside a hashtable where entries
        keys are easy handle memory address converted to string
       will be used when checking for multi_perform completed transfers */
    char easy_key[18];
    sprintf(easy_key, "%p", conn->easy_handle);
    conn->easy_key = strdup(easy_key);
    assert(NULL != conn->easy_key);

    hashtable_set(utils->easy_hashtable, conn->easy_key, conn);

    conn->p_object = p_object; //save object for when load_cb is executed
  }
  
  /* if method is SYNC (default), then exec current conn's easy_handle with
      easy_perform() in a blocking manner.

     if method is SCHEDULE, then add current's conn easy_handle to the multi stack,
      and wait until concord_dispatch() is called for asynchronous execution */
  (*utils->method_cb)(utils, conn); //exec easy_perform() or add handle to multi
}

static void
_concord_perform_scheduler(
  concord_utils_st *utils,
  void **p_object, 
  char conn_key[],
  char endpoint[],
  concord_ld_object_ft *load_cb,
  curl_request_ft *request_cb)
{
  char scheduler_key[5];
  sprintf(scheduler_key, "%ld", utils->active_handles);
  conn_key = scheduler_key;

  struct concord_clist_s *conn = hashtable_get(utils->conn_hashtable, conn_key);
  if (NULL == conn){
    /* didn't find connection node, create a new one */
    _concord_clist_append(utils, &conn, endpoint);
    assert(NULL != conn);

    conn->conn_key = strdup(conn_key);
    assert(NULL != conn->conn_key);
    
    hashtable_set(utils->conn_hashtable, conn->conn_key, conn);

    /* this stores connection node inside a hashtable where entries
        keys are easy handle memory address converted to string
       will be used when checking for multi_perform completed transfers */
    char easy_key[18];
    sprintf(easy_key, "%p", conn->easy_handle);
    conn->easy_key = strdup(easy_key);
    assert(NULL != conn->easy_key);

    hashtable_set(utils->easy_hashtable, conn->easy_key, conn);
  }

  (*request_cb)(utils, conn, endpoint); //register desired request to the easy_handle

  conn->load_cb = load_cb;

  conn->p_object = p_object; //save object for when load_cb is executed
  
  /* if method is SYNC (default), then exec current conn's easy_handle with
      easy_perform() in a blocking manner.

     if method is SCHEDULE, then add current's conn easy_handle to the multi stack,
      and wait until concord_dispatch() is called for asynchronous execution */
  (*utils->method_cb)(utils, conn); //exec easy_perform() or add handle to multi
}

static void
_concord_set_curl_easy(concord_utils_st *utils, struct concord_clist_s *conn)
{
  CURLcode res = curl_easy_perform(conn->easy_handle);
  logger_excep(CURLE_OK != res, curl_share_strerror(res));

  if (NULL != conn->chunk.response){
    //logger_throw(conn->chunk.response);
    (*conn->load_cb)(conn->p_object, &conn->chunk);

    conn->p_object = NULL;
    concord_free(conn->chunk.response);
    conn->chunk.size = 0;
  }

  //@todo sleep value shouldn't be hard-coded
  WAITMS(300);
}

static void
_concord_set_curl_multi(concord_utils_st *utils, struct concord_clist_s *conn)
{
  if (NULL != conn->easy_handle){
    curl_multi_add_handle(utils->multi_handle, conn->easy_handle);
    ++utils->active_handles;

    /* dispatch instantly if hit max active handles */
    if (SCHEDULE_MAX_ACTIVE == utils->active_handles){
      concord_dispatch(utils);
    }
  }
}

/* wrapper around curl_multi_perform() , using select() */
void
concord_dispatch(concord_utils_st *utils)
{
  int still_running = 0; /* keep number of running handles */

  /* we start some action by calling perform right away */
  curl_multi_perform(utils->multi_handle, &still_running);
  while (still_running) {
    int rc; /* select() return code */

    fd_set fdread;
    fd_set fdwrite;
    fd_set fdexcep;
    int maxfd = -1;

    long curl_timeo = -1;

    FD_ZERO(&fdread);
    FD_ZERO(&fdwrite);
    FD_ZERO(&fdexcep);

    /* set a suitable timeout to play around with */
    struct timeval timeout = {.tv_sec = 1, .tv_usec = 0};

    curl_multi_timeout(utils->multi_handle, &curl_timeo);
    if (curl_timeo >= 0){
      timeout.tv_sec = curl_timeo / 1000;
      if (timeout.tv_sec > 1){
        timeout.tv_sec = 1;
      } else {
        timeout.tv_usec = (curl_timeo % 1000) * 1000;
      }
    }

    /*curl_multi_fdset() return code, get file descriptor from the
        transfers*/
    CURLMcode mc = curl_multi_fdset(utils->multi_handle, &fdread, &fdwrite, &fdexcep, &maxfd);

    if (CURLM_OK != mc){
      fprintf(stderr, "curl_multi_fdset() failed, code %d.\n", mc);
      break;
    }

    /* On success the value of maxfd is guaranteed to be >= -1. We call
        select(maxfd+1, ...); specially in case of (maxfd == -1) there
        are no fds ready yet so we call select(0, ...) --or Sleep() on
        Windows-- to sleep 100ms, which is the minimum suggested value
        in curl_multi_fdset() doc. */

    if (-1 == maxfd){
      //@todo sleep value shouldn't be hard-coded
      WAITMS(100);
      rc = 0;
    } else {
      /* Note that ons some platforms 'timeout' may be modified by
          select(). If you need access to the original value save a
          copy beforehand */
      rc = select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);
    }

    switch(rc) {
    case -1:
        /* select error */
        break;
    case 0: /* timeout */
    default: /* action */
        curl_multi_perform(utils->multi_handle, &still_running);
        break;
    }
  }

  /* See how the transfers went */
  CURLMsg *msg; /* for picking up messages with the transfer status */
  int msgs_left; /*how many messages are left */
  while ((msg = curl_multi_info_read(utils->multi_handle, &msgs_left))){
    if (CURLMSG_DONE != msg->msg) continue;

    /* Find out which handle this message is about */
    char easy_key[18];
    sprintf(easy_key, "%p", msg->easy_handle);
    struct concord_clist_s *conn = hashtable_get(utils->easy_hashtable, easy_key);
    assert (NULL != conn);

    /* execute load callback to perform change in object */
    if (NULL != conn->chunk.response){
      //logger_throw(conn->chunk.response);
      (*conn->load_cb)(conn->p_object, &conn->chunk);

      conn->p_object = NULL;
      concord_free(conn->chunk.response);
      conn->chunk.size = 0;
    }

    /* @todo this probably should be inside the above if condition */
    curl_multi_remove_handle(utils->multi_handle, conn->easy_handle);
    --utils->active_handles;
  }

  assert(0 == utils->active_handles);
}

void
concord_request_method(concord_st *concord, concord_request_method_et method)
{
  switch (method){
  case SCHEDULE:
      concord->utils->method = SCHEDULE;
      concord->utils->method_cb = &_concord_set_curl_multi;
      break;
  case SYNC:
      concord->utils->method = SYNC;
      concord->utils->method_cb = &_concord_set_curl_easy;
      break;
  default:
      logger_throw("undefined request method");
      exit(EXIT_FAILURE);
  }
}

void
Concord_GET(concord_utils_st *utils, struct concord_clist_s *conn, char endpoint[])
{
  char base_url[MAX_URL_LENGTH] = BASE_URL;

  curl_easy_setopt(conn->easy_handle, CURLOPT_URL, strcat(base_url, endpoint));
  curl_easy_setopt(conn->easy_handle, CURLOPT_HTTPGET, 1L);
}

void
Concord_POST(concord_utils_st *utils, struct concord_clist_s *conn, char endpoint[])
{
  char base_url[MAX_URL_LENGTH] = BASE_URL;

  curl_easy_setopt(conn->easy_handle, CURLOPT_URL, strcat(base_url, endpoint));
  curl_easy_setopt(conn->easy_handle, CURLOPT_POST, 1L);
}

/* @todo create distinction between bot and user token */
static struct curl_slist*
_concord_init_request_header(concord_utils_st *utils)
{
  char auth_header[MAX_HEADER_LENGTH] = "Authorization: Bot "; 

  struct curl_slist *new_header = NULL;
  new_header = curl_slist_append(new_header,"X-RateLimit-Precision: millisecond");
  assert(NULL != new_header);

  new_header = curl_slist_append(new_header, strcat(auth_header, utils->token));
  assert(NULL != new_header);

  new_header = curl_slist_append(new_header,"User-Agent: concord (http://github.com/LucasMull/concord, v0.0)");
  assert(NULL != new_header);

  new_header = curl_slist_append(new_header,"Content-Type: application/json");
  assert(NULL != new_header);

  return new_header;
}

static concord_utils_st*
_concord_utils_init(char token[])
{
  concord_utils_st *new_utils = concord_malloc(sizeof *new_utils + strlen(token));
  strncpy(new_utils->token, token, strlen(token)-1);

  new_utils->header = _concord_init_request_header(new_utils);

  new_utils->method = SYNC;
  new_utils->method_cb = &_concord_set_curl_easy;

  new_utils->easy_hashtable = hashtable_init();
  hashtable_build(new_utils->easy_hashtable, UTILS_HASHTABLE_SIZE);

  new_utils->conn_hashtable = hashtable_init();
  hashtable_build(new_utils->conn_hashtable, UTILS_HASHTABLE_SIZE);

  new_utils->multi_handle = curl_multi_init();

  return new_utils;
}

static void
_concord_utils_destroy(concord_utils_st *utils)
{
  curl_slist_free_all(utils->header);
  curl_multi_cleanup(utils->multi_handle);
  hashtable_destroy(utils->easy_hashtable);
  hashtable_destroy(utils->conn_hashtable);
  _concord_clist_free_all(utils->conn_list);

  concord_free(utils);
}

void
Concord_perform_request(
  concord_utils_st *utils, 
  void **p_object, 
  char conn_key[],
  char endpoint[], 
  concord_ld_object_ft *load_cb, 
  curl_request_ft *request_cb)
{
  switch (utils->method){
  case SCHEDULE:
      _concord_perform_scheduler(
                      utils,
                      p_object,
                      conn_key,
                      endpoint,
                      load_cb,
                      request_cb);
      break;
  case SYNC:
      _concord_perform_sync(
                 utils,
                 p_object,
                 conn_key,
                 endpoint,
                 load_cb,
                 request_cb);
      break;
  default:
      logger_throw("undefined request method");
      exit(EXIT_FAILURE);
  }
}

concord_st*
concord_init(char token[])
{
  concord_st *new_concord = concord_malloc(sizeof *new_concord);

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

  concord_free(concord);
}

void
concord_global_init(){
  curl_global_init(CURL_GLOBAL_DEFAULT);
}

void
concord_global_cleanup(){
  curl_global_cleanup();
}
