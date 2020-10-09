#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
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
_discord_clist_append_nodup(discord_utils_st *utils, struct discord_clist_s *conn_list)
{
  struct discord_clist_s *last;
  struct discord_clist_s *new_node;

  new_node = discord_malloc(sizeof *new_node);

  new_node->easy_handle = _discord_curl_easy_init(utils, &new_node->chunk);

  if (!conn_list) return new_node;

  last = _discord_clist_get_last(conn_list);
  last->next = new_node;

  return conn_list;
}

struct discord_clist_s*
discord_clist_append(discord_utils_st *utils, struct discord_clist_s *conn_list)
{
  conn_list = _discord_clist_append_nodup(utils, conn_list);

  return conn_list;
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
    discord_free(node);
    node = next;
  } while (next);
}

static void
_discord_request_easy(discord_utils_st *utils, struct discord_clist_s *conn_list)
{
  CURLcode res = curl_easy_perform(conn_list->easy_handle);
  if (CURLE_OK != res){
    fprintf(stderr, "\n%s\n\n", curl_share_strerror(res));
    exit(EXIT_FAILURE);
  }
}

/* @todo make this append a new clist to existing one */
static void
_discord_request_multi(discord_utils_st *utils, struct discord_clist_s *conn_list)
{
  CURL *easy_handle_clone = conn_list->easy_handle;

  if (NULL != easy_handle_clone){
    curl_multi_add_handle(utils->multi_handle, easy_handle_clone);
  }
}

void
discord_request_method(discord_st *discord, discord_request_method_et method)
{
  switch (method){
  case ASYNC:
      discord->utils->method = ASYNC;
      discord->utils->method_cb = &_discord_request_multi;
      break;
  case SYNC:
      discord->utils->method = SYNC;
      discord->utils->method_cb = &_discord_request_easy;
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
  new_utils->method_cb = &_discord_request_easy;

  new_utils->multi_handle = curl_multi_init();

  return new_utils;
}

static void
_discord_utils_destroy(discord_utils_st *utils)
{
  curl_slist_free_all(utils->header);
  curl_multi_cleanup(utils->multi_handle);

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
