#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <curl/curl.h>

#include "libconcord.h"

struct memory_s {
  char *response;
  size_t size;
};

void
discord_free(void *ptr)
{
  if(NULL != ptr){
    free(ptr);
    ptr = NULL;
  } 
}

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

static size_t
_discord_utils_response_cb(char *content, size_t size, size_t nmemb, void *p_userdata)
{
  size_t realsize = size * nmemb;
  struct memory_s *chunk = (struct memory_s*)p_userdata;

  char *tmp = realloc(chunk->response, chunk->size + realsize + 1);

  if (tmp == NULL) return 0;

  chunk->response = tmp;
  memcpy(&chunk->response[chunk->size], content, realsize);
  chunk->size += realsize;
  chunk->response[chunk->size] = '\0';

  return realsize;
}

CURL*
discord_easy_default_init(discord_utils_st *utils)
{
  CURL *new_easy_handle = curl_easy_init();
  assert(NULL != new_easy_handle);

  curl_easy_setopt(new_easy_handle, CURLOPT_HTTPHEADER, utils->header);
  curl_easy_setopt(new_easy_handle, CURLOPT_FAILONERROR, 1L);
  curl_easy_setopt(new_easy_handle, CURLOPT_VERBOSE, 1L);

  return new_easy_handle;
}

static char*
_discord_request_easy(discord_st *discord, CURL *easy_handle)
{
  struct memory_s chunk = {NULL};

  // SET CURL_EASY CALLBACK //
  curl_easy_setopt(easy_handle, CURLOPT_WRITEFUNCTION, &_discord_utils_response_cb);
  curl_easy_setopt(easy_handle, CURLOPT_WRITEDATA, &chunk);

  CURLcode res = curl_easy_perform(easy_handle);
  if (CURLE_OK != res){
    fprintf(stderr, "\n%s\n\n", curl_share_strerror(res));
    exit(EXIT_FAILURE);
  }

  //UNCOMMENT TO SEE JSON RESPONSE
  //fprintf(stderr, "\n\n%s\n\n", utils->response);

  return chunk.response;
}

static char*
_discord_request_multi(discord_st *discord, CURL *easy_handle)
{
  curl_multi_add_handle(discord->multi_handle, easy_handle);

  return NULL;
}

void
discord_request_method(discord_st *discord, discord_request_method_et method)
{
  switch (method){
  case ASYNC:
      discord->request_method = &_discord_request_multi;
      break;
  case SYNC:
      discord->request_method = &_discord_request_easy;
      break;
  default:
      fprintf(stderr, "\nERROR: undefined request method\n");
      exit(EXIT_FAILURE);
  }
}

char*
discord_request_get(discord_st *discord, CURL *easy_handle, char url_route[])
{
  char base_url[MAX_URL_LENGTH] = BASE_URL;

  curl_easy_setopt(easy_handle, CURLOPT_URL, strcat(base_url, url_route));
  curl_easy_setopt(easy_handle, CURLOPT_HTTPGET, 1L);
  char *response = (*discord->request_method)(discord, easy_handle);

  return response;
}

char*
discord_request_post(discord_st *discord, CURL *easy_handle, char url_route[])
{
  char base_url[MAX_URL_LENGTH] = BASE_URL;

  curl_easy_setopt(easy_handle, CURLOPT_URL, strcat(base_url, url_route));
  curl_easy_setopt(easy_handle, CURLOPT_POST, 1L);
  char *response = (*discord->request_method)(discord, easy_handle);

  return response;
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

  return new_utils;
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

  new_discord->request_method = &_discord_request_easy;

  new_discord->multi_handle = curl_multi_init();

  return new_discord;
}

void
discord_cleanup(discord_st *discord)
{
  discord_channel_destroy(discord->channel);
  discord_guild_destroy(discord->guild);
  discord_user_destroy(discord->user);
  discord_user_destroy(discord->client);

  curl_slist_free_all(discord->utils->header);

  discord_free(discord->utils);

  curl_multi_cleanup(discord->multi_handle);

  discord_free(discord);

  curl_global_cleanup();
}
