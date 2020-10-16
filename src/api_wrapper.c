#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

//#include <curl/curl.h>
//#include <libjscon.h>

#include <libconcord.h>

#include "hashtable.h"
#include "api_wrapper_private.h"
#include "logger.h"

/* code excerpt taken from
  https://raw.githubusercontent.com/Michaelangel007/buddhabrot/master/buddhabrot.cpp */
#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #define NOMINMAX
  #include <Windows.h> // Windows.h -> WinDef.h defines min() max()

  typedef struct timeval {
    long tv_sec;
    long tv_usec;
  } timeval;

  int gettimeofday(struct timeval *tp, struct timezone *tzp)
  {
    // FILETIME JAN 1 1970 00:00:00
    /* Note: some broken version only have 8 trailing zero's, the corret epoch has 9
        trailing zero's */
    static cont uint64_t EPOCH = ((uint64_t) 116444736000000000ULL);

    SYSTEMTIME  nSystemTime;
    FILETIME    nFileTime;
    uint64_t    ntime;

    GetSystemTime( &nSystemTime );
    SystemTimeToFileTime( &nSystemTime, &nFileTime );
    nTime = ((uint64_t)nFileTime.dwLowDateTime );
    nTime += ((uint64_t)nFileTime.dwHighDateTime) << 32;

    tp->tv_sec = (long) ((nTime - EPOCH) / 10000000L);
    tp->tv_sec = (long) (nSystemTime.wMilliseconds * 1000);

    return 0;
  }

  #define WAITMS(t) Sleep(t)
#else 
  #include <sys/time.h>
  #include <unistd.h>

  #define WAITMS(t) usleep((t)*1000)
#endif

static double
_concord_parse_ratelimit(struct concord_ratelimit_s *ratelimit, _Bool use_clock)
{
  if (true == use_clock || !strtod(ratelimit->reset_after, NULL)){
    struct timeval te;

    gettimeofday(&te, NULL); //get current time
    
    double utc = te.tv_sec*1000 + te.tv_usec/1000; //calculate milliseconds
    double reset = strtod(ratelimit->reset, NULL) * 1000;

    return reset - utc + 1000;
  }

  return strtod(ratelimit->reset_after, NULL);
}

#define XRL_BUCKET      "x-ratelimit-bucket"
#define XRL_LIMIT       "x-ratelimit-limit"
#define XRL_REMAINING   "x-ratelimit-remaining"
#define XRL_RESET       "x-ratelimit-reset"
#define XRL_RESET_AFTER "x-ratelimit-reset-after"

static size_t
_concord_curl_header_cb(char *content, size_t size, size_t nmemb, void *p_userdata)
{
  int realsize = size * nmemb;
  struct concord_ratelimit_s *ratelimit = (struct concord_ratelimit_s*)p_userdata;

  int len=0;
  while (!iscntrl(content[len]))
  {
    if (':' != content[len]){
      ++len;
      continue;
    } 

    if (0 != strncmp(content, "x-ratelimit", 11)){ 
      break;
    }

    char key[30];
    strncpy(key, content, len);
    key[len] = '\0';

    char **rl_field = hashtable_get(ratelimit->header_hashtable, key);
    concord_free(*rl_field);

    
    *rl_field = strndup(&content[len+2], realsize - len+2);
    assert(NULL != *rl_field);
    break;
  }


  return (size_t)realsize;
}

static size_t
_concord_curl_body_cb(char *content, size_t size, size_t nmemb, void *p_userdata)
{
  size_t realsize = size * nmemb;
  struct curl_response_s *response_body = (struct curl_response_s*)p_userdata;

  char *tmp = realloc(response_body->str, response_body->size + realsize + 1);

  if (tmp == NULL) return 0;

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
  curl_easy_setopt(new_easy_handle, CURLOPT_VERBOSE, 1L);

  // SET CURL_EASY CALLBACKS //
  curl_easy_setopt(new_easy_handle, CURLOPT_WRITEFUNCTION, &_concord_curl_body_cb);
  curl_easy_setopt(new_easy_handle, CURLOPT_WRITEDATA, &conn->response_body);

  curl_easy_setopt(new_easy_handle, CURLOPT_HEADERFUNCTION, &_concord_curl_header_cb);
  curl_easy_setopt(new_easy_handle, CURLOPT_HEADERDATA, &utils->ratelimit);

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

    /* share resources between SYNC type easy_handles */
    curl_easy_setopt(conn->easy_handle, CURLOPT_SHARE, utils->easy_share);

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
  }

  conn->p_object = p_object; //save object for when load_cb is executed
  
  /* if method is SYNC (default), then exec current conn's easy_handle with
      easy_perform() in a blocking manner. */

  (*utils->method_cb)(utils, conn); //exec easy_perform() or add handle to multi
}

static void
_concord_perform_schedule(
  concord_utils_st *utils,
  void **p_object, 
  char conn_key[],
  char endpoint[],
  concord_ld_object_ft *load_cb,
  curl_request_ft *request_cb)
{
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
  
  /* if method is SCHEDULE, then add current's conn easy_handle to the multi stack,
      and wait until concord_dispatch() is called for asynchronous execution */
  (*utils->method_cb)(utils, conn); //exec easy_perform() or add handle to multi
}

static void
_concord_set_curl_easy(concord_utils_st *utils, struct concord_clist_s *conn)
{
  CURLcode ecode = curl_easy_perform(conn->easy_handle);
  logger_excep(CURLE_OK != ecode, curl_easy_strerror(ecode));

  if (NULL != conn->response_body.str){
    /* @todo for some reason only getting a single header when
        doing blocking, find out why */
    double delay_ms;
    if (0 != strtol(utils->ratelimit.remaining, NULL, 10)){
      delay_ms = _concord_parse_ratelimit(&utils->ratelimit, false);
    } else {
      delay_ms = 0;
    }

    WAITMS(delay_ms);

    //logger_throw(conn->response_body.str);
    (*conn->load_cb)(conn->p_object, &conn->response_body);

    conn->p_object = NULL;

    concord_free(conn->response_body.str);
    conn->response_body.size = 0;
  }
}

static void
_concord_set_curl_multi(concord_utils_st *utils, struct concord_clist_s *conn)
{
  if (NULL != conn->easy_handle){
    curl_multi_add_handle(utils->multi_handle, conn->easy_handle);
    ++utils->active_handles;

    /* dispatch instantly if hit max active handles limit */
    if (SCHEDULE_MAX_ACTIVE == utils->active_handles){
      concord_dispatch(utils);
    }
  }
}

/* wrapper around curl_multi_perform() , using poll() */
void
concord_dispatch(concord_utils_st *utils)
{
  int transfers_running = 0; /* keep number of running handles */
  int repeats = 0;
  double delay_ms = 0;
  do {
    CURLMcode mcode;
    int numfds;

    mcode = curl_multi_perform(utils->multi_handle, &transfers_running);

    if (CURLM_OK == mcode){
      /* wait for activity, timeout or "nothing" */
      mcode = curl_multi_wait(utils->multi_handle, NULL, 0, delay_ms, &numfds);
    }

    if (CURLM_OK != mcode){
      logger_throw(curl_multi_strerror(mcode));
      break;
    }

    /* numfds being zero means either a timeout or no file descriptor to
        wait for. Try timeout on first occurrences, then assume no file
        descriptors and no file descriptors to wait for mean wait for
        100 milliseconds. */
    if (0 == numfds){
      ++repeats; /* count number of repeated zero numfds */
      if (repeats > 1){
        WAITMS(100); /* sleep 100 milliseconds */
      }
    } else {
      /* @todo segmentation fault occurring when doing valgrind and utils->ratelimit
          has a null attribute being checked */
      if (utils->ratelimit.remaining && 0 != strtol(utils->ratelimit.remaining, NULL, 10)){
        delay_ms = _concord_parse_ratelimit(&utils->ratelimit, true);
      } else {
        delay_ms = 0;
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
    struct concord_clist_s *conn = hashtable_get(utils->easy_hashtable, easy_key);
    assert (NULL != conn);

    /* execute load callback to perform change in object */
    if (NULL != conn->response_body.str){
      //logger_throw(conn->response_body.str);
      (*conn->load_cb)(conn->p_object, &conn->response_body);

      conn->p_object = NULL;

      concord_free(conn->response_body.str);
      conn->response_body.size = 0;
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
      logger_throw("ERROR: undefined request method");
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

static concord_utils_st*
_concord_utils_init(char token[])
{
  concord_utils_st *new_utils = concord_malloc(sizeof *new_utils + strlen(token));
  strncpy(new_utils->token, token, strlen(token)-1);

  new_utils->request_header = _concord_init_request_header(new_utils);

  new_utils->multi_handle = curl_multi_init();

  /* @todo i need to benchmark this to see if there's actual benefit */
  new_utils->easy_share = curl_share_init();
  curl_share_setopt(new_utils->easy_share, CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE);
  curl_share_setopt(new_utils->easy_share, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
  curl_share_setopt(new_utils->easy_share, CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT);

  new_utils->easy_hashtable = hashtable_init();
  hashtable_build(new_utils->easy_hashtable, UTILS_HASHTABLE_SIZE);

  new_utils->conn_hashtable = hashtable_init();
  hashtable_build(new_utils->conn_hashtable, UTILS_HASHTABLE_SIZE);

  new_utils->method = SYNC;
  new_utils->method_cb = &_concord_set_curl_easy;

  /* @todo turn into a function */
  struct concord_ratelimit_s *ratelimit = &new_utils->ratelimit;
  ratelimit->header_hashtable = hashtable_init();
  hashtable_build(ratelimit->header_hashtable, 15);

  char *key; 
  key = strdup(XRL_BUCKET);
  assert(NULL != key);
  hashtable_set(ratelimit->header_hashtable, key, &ratelimit->bucket);

  key = strdup(XRL_LIMIT);
  assert(NULL != key);
  hashtable_set(ratelimit->header_hashtable, key, &ratelimit->limit);

  key = strdup(XRL_REMAINING);
  assert(NULL != key);
  hashtable_set(ratelimit->header_hashtable, key, &ratelimit->remaining);

  key = strdup(XRL_RESET);
  assert(NULL != key);
  hashtable_set(ratelimit->header_hashtable, key, &ratelimit->reset);

  key = strdup(XRL_RESET_AFTER);
  assert(NULL != key);
  hashtable_set(ratelimit->header_hashtable, key, &ratelimit->reset_after);

  return new_utils;
}

static void
_concord_utils_destroy(concord_utils_st *utils)
{
  curl_slist_free_all(utils->request_header);
  curl_multi_cleanup(utils->multi_handle);
  _concord_clist_free_all(utils->conn_list);
  curl_share_cleanup(utils->easy_share);
  hashtable_destroy(utils->easy_hashtable);
  hashtable_destroy(utils->conn_hashtable);

  hashtable_entry_st *entry;
  entry = hashtable_get_entry(utils->ratelimit.header_hashtable, XRL_BUCKET);
  concord_free(utils->ratelimit.bucket);
  free(entry->key);

  entry = hashtable_get_entry(utils->ratelimit.header_hashtable, XRL_LIMIT);
  concord_free(utils->ratelimit.limit);
  free(entry->key);

  entry = hashtable_get_entry(utils->ratelimit.header_hashtable, XRL_REMAINING);
  concord_free(utils->ratelimit.remaining);
  free(entry->key);

  entry = hashtable_get_entry(utils->ratelimit.header_hashtable, XRL_RESET);
  concord_free(utils->ratelimit.reset);
  free(entry->key);

  entry = hashtable_get_entry(utils->ratelimit.header_hashtable, XRL_RESET_AFTER);
  concord_free(utils->ratelimit.reset_after);
  free(entry->key);

  hashtable_destroy(utils->ratelimit.header_hashtable);

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
   {
      char task_key[15];
      sprintf(task_key, "ScheduleTask#%ld", utils->active_handles);

      _concord_perform_schedule(
                      utils,
                      p_object,
                      task_key,
                      endpoint,
                      load_cb,
                      request_cb);
      break;
   }
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
      logger_throw("ERROR: undefined request method");
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
