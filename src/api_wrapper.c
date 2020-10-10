#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

/* somewhat unix-specific */
#include <sys/time.h>
#include <unistd.h>

#include <curl/curl.h>

#include "libconcord.h"

/* @todo instead of exit(), it should throw the error
    somewhere */
//this is redefined as a macro
void*
__discord_malloc(size_t size, unsigned long line)
{
  void *ptr = calloc(1, size);

  if (NULL == ptr){
    fprintf(stderr, "[%s:%lu] Out of memory(%lu bytes)\n",
              __FILE__, line, (unsigned long)size);
    exit(EXIT_FAILURE);
  }

  return ptr;
}

//this is redefined as a macro
void
__discord_free(void **p_ptr)
{
  if(NULL != p_ptr){
    free(*p_ptr);
    *p_ptr = NULL;
  } 
}

static size_t
_discord_curl_response_cb(char *content, size_t size, size_t nmemb, void *p_userdata)
{
  size_t realsize = size * nmemb;
  struct curl_memory_s *chunk = (struct curl_memory_s*)p_userdata;

  char *tmp = realloc(chunk->response, chunk->size + realsize + 1);

  if (tmp == NULL) return 0;

  chunk->response = tmp;
  memcpy((char*)chunk->response + chunk->size, content, realsize);
  chunk->size += realsize;
  chunk->response[chunk->size] = '\0';

  return realsize;
}

CURL*
_discord_curl_easy_init(discord_utils_st *utils, struct curl_memory_s *chunk)
{
  CURL *new_easy_handle = curl_easy_init();
  assert(NULL != new_easy_handle);

  curl_easy_setopt(new_easy_handle, CURLOPT_HTTPHEADER, utils->header);
  curl_easy_setopt(new_easy_handle, CURLOPT_FAILONERROR, 1L);
  curl_easy_setopt(new_easy_handle, CURLOPT_VERBOSE, 1L);

  // SET CURL_EASY CALLBACK //
  curl_easy_setopt(new_easy_handle, CURLOPT_WRITEFUNCTION, &_discord_curl_response_cb);
  curl_easy_setopt(new_easy_handle, CURLOPT_WRITEDATA, (void*)chunk);

  return new_easy_handle;
}

static struct discord_clist_s*
_discord_clist_get_last(struct discord_clist_s *conn_list)
{
  if (!conn_list) return NULL;

  struct discord_clist_s *iter = conn_list;
  while (NULL != iter->next){
    iter = iter->next;
  }

  return iter;
}

static struct discord_clist_s*
_discord_clist_append_nodup(discord_utils_st *utils, struct discord_clist_s **p_new_node)
{
  struct discord_clist_s *last;
  struct discord_clist_s *new_node = discord_malloc(sizeof *new_node);

  new_node->easy_handle = _discord_curl_easy_init(utils, &new_node->chunk);

  if (NULL != p_new_node){
    *p_new_node = new_node;
  }

  if (NULL == utils->conn_list){
    utils->conn_list = new_node;
    return new_node;
  }

  last = _discord_clist_get_last(utils->conn_list);
  last->next = new_node;

  return utils->conn_list;
}

struct discord_clist_s*
discord_clist_append(discord_utils_st *utils, struct discord_clist_s **p_new_node){
  return _discord_clist_append_nodup(utils, p_new_node);
}

void
discord_clist_free_all(struct discord_clist_s *conn_list)
{
  if (!conn_list) return;

  struct discord_clist_s *node = conn_list;
  struct discord_clist_s *next;
  do {
    next = node->next;
    curl_easy_cleanup(node->easy_handle);
    discord_free(node->primary_key);
    discord_free(node->secondary_key);
    discord_free(node);
    node = next;
  } while (next);
}

struct discord_clist_s*
discord_get_conn( discord_utils_st *utils, char key[], discord_load_ft *load_cb)
{
  struct discord_clist_s *node = hashtable_get(utils->conn_hashtable, key);

  /* found connection node, return it */
  if (NULL != node) return node;

  /* didn't find connection node, create a new one and return it */
  struct discord_clist_s *new_node;
  node = discord_clist_append(utils, &new_node);
  assert(NULL != node && NULL != new_node);

  new_node->load_cb = load_cb;
  assert(NULL != new_node->load_cb);

  new_node->primary_key = strdup(key);
  assert(NULL != new_node->primary_key);
  /* this stores connection node inside object's specific hashtable
      using the node key (given at this function parameter) */
  hashtable_set(utils->conn_hashtable, new_node->primary_key, new_node);

  /* this stores connection node inside discord's general hashtable
      using easy handle's memory address converted to string as key.
     will be used when checking for multi_perform completed transfers */
  char addr_key[18];
  sprintf(addr_key, "%p", new_node->easy_handle);
  new_node->secondary_key = strdup(addr_key);
  assert(NULL != new_node->secondary_key);

  hashtable_set(utils->easy_hashtable, new_node->secondary_key, new_node);

  return new_node;
}

static void
_discord_set_curl_easy(discord_utils_st *utils, struct discord_clist_s *conn_list)
{
  CURLcode res = curl_easy_perform(conn_list->easy_handle);
  if (CURLE_OK != res){
    fprintf(stderr, "\n%s\n\n", curl_share_strerror(res));
    exit(EXIT_FAILURE);
  }
}

static void
_discord_set_curl_multi(discord_utils_st *utils, struct discord_clist_s *conn_list)
{
  if (NULL != conn_list->easy_handle){
    curl_multi_add_handle(utils->multi_handle, conn_list->easy_handle);
  }
}

/* wrapper around curl_multi_perform() */
void
discord_dispatch(discord_st *discord)
{
  discord_utils_st *utils = discord->utils;

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
#ifdef _WIN32
      Sleep(100);
      rc = 0;
#else
      /* Portable sleep for platforms other than Windows. */
      struct timeval wait = { 0, 100 * 1000 }; /* 100ms */
      rc = select(0, NULL, NULL, NULL, &wait);
#endif
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
  while (NULL != (msg = curl_multi_info_read(utils->multi_handle, &msgs_left))){
    if (CURLMSG_DONE != msg->msg){
      continue;
    }

    /* Find out which handle this message is about */
    char addr_key[18];
    sprintf(addr_key, "%p", msg->easy_handle);
    struct discord_clist_s *conn = hashtable_get(utils->easy_hashtable, addr_key);
    assert (NULL != conn);

    (*conn->load_cb)(conn->p_object, &conn->chunk); /* load object */
    conn->p_object = NULL;

    curl_multi_remove_handle(utils->multi_handle, conn->easy_handle);
  }
}

void
discord_request_method(discord_st *discord, discord_request_method_et method)
{
  switch (method){
  case SCHEDULE:
      discord->utils->method = SCHEDULE;
      discord->utils->method_cb = &_discord_set_curl_multi;
      break;
  case SYNC:
      discord->utils->method = SYNC;
      discord->utils->method_cb = &_discord_set_curl_easy;
      break;
  default:
      fprintf(stderr, "\nERROR: undefined request method\n");
      exit(EXIT_FAILURE);
  }
}

void
discord_request_get(discord_utils_st *utils, struct discord_clist_s *conn_list, char url_route[])
{
  char base_url[MAX_URL_LENGTH] = BASE_URL;

  curl_easy_setopt(conn_list->easy_handle, CURLOPT_URL, strcat(base_url, url_route));
  curl_easy_setopt(conn_list->easy_handle, CURLOPT_HTTPGET, 1L);

  (*utils->method_cb)(utils, conn_list);
}

void
discord_request_post(discord_utils_st *utils, struct discord_clist_s *conn_list, char url_route[])
{
  char base_url[MAX_URL_LENGTH] = BASE_URL;

  curl_easy_setopt(conn_list->easy_handle, CURLOPT_URL, strcat(base_url, url_route));
  curl_easy_setopt(conn_list->easy_handle, CURLOPT_POST, 1L);

  (*utils->method_cb)(utils, conn_list);
}

/* @todo create distinction between bot and user token */
static struct curl_slist*
_discord_init_request_header(discord_utils_st *utils)
{
  char auth_header[MAX_HEADER_LENGTH] = "Authorization: Bot "; 

  struct curl_slist *new_header = NULL;
  new_header = curl_slist_append(new_header,"X-RateLimit-Precision: millisecond");
  assert(NULL != new_header);

  new_header = curl_slist_append(new_header, strcat(auth_header, utils->bot_token));
  assert(NULL != new_header);

  new_header = curl_slist_append(new_header,"User-Agent: concord (http://github.com/LucasMull/concord, v0.0)");
  assert(NULL != new_header);

  new_header = curl_slist_append(new_header,"Content-Type: application/json");
  assert(NULL != new_header);

  return new_header;
}

static discord_utils_st*
_discord_utils_init(char bot_token[])
{
  discord_utils_st *new_utils = discord_malloc(sizeof *new_utils);
  strncpy(new_utils->bot_token, bot_token, BOT_TOKEN_LENGTH-1);

  new_utils->header = _discord_init_request_header(new_utils);

  new_utils->method = SYNC;
  new_utils->method_cb = &_discord_set_curl_easy;

  new_utils->easy_hashtable = hashtable_init();
  hashtable_build(new_utils->easy_hashtable, UTILS_HASHTABLE_SIZE);

  new_utils->conn_hashtable = hashtable_init();
  hashtable_build(new_utils->conn_hashtable, UTILS_HASHTABLE_SIZE);

  new_utils->multi_handle = curl_multi_init();

  return new_utils;
}

static void
_discord_utils_destroy(discord_utils_st *utils)
{
  curl_slist_free_all(utils->header);
  curl_multi_cleanup(utils->multi_handle);
  hashtable_destroy(utils->easy_hashtable);
  hashtable_destroy(utils->conn_hashtable);
  discord_clist_free_all(utils->conn_list);

  discord_free(utils);
}

discord_st*
discord_init(char bot_token[])
{
  curl_global_init(CURL_GLOBAL_DEFAULT);

  discord_st *new_discord = discord_malloc(sizeof *new_discord);

  new_discord->utils = _discord_utils_init(bot_token);

  new_discord->channel = discord_channel_init(new_discord->utils);
  new_discord->guild = discord_guild_init(new_discord->utils);
  new_discord->user = discord_user_init(new_discord->utils);
  new_discord->client = discord_user_init(new_discord->utils);

  return new_discord;
}

void
discord_cleanup(discord_st *discord)
{
  discord_channel_destroy(discord->channel);
  discord_guild_destroy(discord->guild);
  discord_user_destroy(discord->user);
  discord_user_destroy(discord->client);
  _discord_utils_destroy(discord->utils);

  discord_free(discord);

  curl_global_cleanup();
}
