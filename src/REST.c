#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <assert.h>

#include "REST.h"

static size_t
discord_utils_response_cb(char *content, size_t size, size_t nmemb, void *p_userdata)
{
  char *response = (char*)p_userdata;
  strncat(response, content, size * nmemb);
  return size * nmemb;
}

CURL*
curl_easy_custom_init(discord_utils_st *utils)
{
  CURL *new_easy_handle = curl_easy_init();
  assert(NULL != new_easy_handle);

  curl_easy_setopt(new_easy_handle, CURLOPT_HTTPHEADER, utils->header);
  curl_easy_setopt(new_easy_handle, CURLOPT_FAILONERROR, 1L);
  curl_easy_setopt(new_easy_handle, CURLOPT_VERBOSE, 1L);

  // SET CURL_EASY CALLBACK //
  curl_easy_setopt(new_easy_handle, CURLOPT_WRITEFUNCTION, &discord_utils_response_cb);
  curl_easy_setopt(new_easy_handle, CURLOPT_WRITEDATA, utils->response);

  return new_easy_handle;
}

void
discord_request_get(CURL *easy_handle, discord_utils_st *utils)
{
  char base_url[MAX_URL_LENGTH] = BASE_URL;
  curl_easy_setopt(easy_handle, CURLOPT_URL, strcat(base_url, utils->url_route));
  curl_easy_setopt(easy_handle, CURLOPT_HTTPGET, 1L);

  *utils->response = '\0';

  CURLcode response = curl_easy_perform(easy_handle);
  if (CURLE_OK != response){
    fprintf(stderr, "\n%s\n\n", curl_share_strerror(response));
    exit(EXIT_FAILURE);
  }

  //UNCOMMENT TO SEE JSON RESPONSE
  //fprintf(stderr, "\n\n%s\n\n", utils->response);
}

void
discord_request_post(CURL *easy_handle, discord_utils_st *utils)
{
  char base_url[MAX_URL_LENGTH] = BASE_URL;
  curl_easy_setopt(easy_handle, CURLOPT_URL, strcat(base_url, utils->url_route));
  curl_easy_setopt(easy_handle, CURLOPT_POST, 1L);

  *utils->response = '\0';

  CURLcode response = curl_easy_perform(easy_handle);
  if (CURLE_OK != response){
    fprintf(stderr, "\n%s\n\n", curl_share_strerror(response));
    exit(EXIT_FAILURE);
  }

  //UNCOMMENT TO SEE JSON RESPONSE
  //fprintf(stderr, "\n\n%s\n\n", utils->response);
}
