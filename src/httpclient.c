#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <assert.h>

#include "httpclient.h"

struct memory_s {
  char *response;
  size_t size;
};

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
curl_easy_custom_init(discord_utils_st *utils)
{
  CURL *new_easy_handle = curl_easy_init();
  assert(NULL != new_easy_handle);

  curl_easy_setopt(new_easy_handle, CURLOPT_HTTPHEADER, utils->header);
  curl_easy_setopt(new_easy_handle, CURLOPT_FAILONERROR, 1L);
  curl_easy_setopt(new_easy_handle, CURLOPT_VERBOSE, 1L);

  return new_easy_handle;
}

char*
discord_request_get(discord_st *discord, CURL *easy_handle, char url_route[])
{
  char base_url[MAX_URL_LENGTH] = BASE_URL;

  struct memory_s chunk = {NULL};

  curl_easy_setopt(easy_handle, CURLOPT_URL, strcat(base_url, url_route));
  curl_easy_setopt(easy_handle, CURLOPT_HTTPGET, 1L);

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

char*
discord_request_post(discord_st *discord, CURL *easy_handle, char url_route[])
{
  char base_url[MAX_URL_LENGTH] = BASE_URL;

  struct memory_s chunk = {NULL};

  curl_easy_setopt(easy_handle, CURLOPT_URL, strcat(base_url, url_route));
  curl_easy_setopt(easy_handle, CURLOPT_POST, 1L);

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
