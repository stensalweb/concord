#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <assert.h>

#include "REST.h"
#include "global_config.h"


size_t
api_response_write_callback(char *content, size_t size, size_t nmemb, void *p_userdata)
{
  char *response = (char*)p_userdata;
  strncat(response, content, size * nmemb);
  return size * nmemb;
}

void
curl_easy_set_write(CURL *easy_handle, api_response_st *buffer)
{
  char auth_header[MAX_HEADER_LENGTH] = "Authorization: Bot "; 
  char base_url[MAX_URL_LENGTH] = BASE_URL;
  struct curl_slist *header = NULL;

  header = curl_slist_append(header,"X-RateLimit-Precision: millisecond");
  assert(NULL != header);
  header = curl_slist_append(header,strcat(auth_header,g_config.bot_token));
  assert(NULL != header);
  header = curl_slist_append(header,"User-Agent: DISCORDc (http://github.com/LucasMull/DISCORDc, v0.0)");
  assert(NULL != header);
  header = curl_slist_append(header,"Content-Type: application/json");
  assert(NULL != header);

  curl_easy_setopt(easy_handle, CURLOPT_HTTPHEADER, header);
  curl_easy_setopt(easy_handle, CURLOPT_URL, strcat(base_url, g_config.url_route));
  //curl_easy_setopt(easy_handle, CURLOPT_FAILONERROR, 1L);
  //curl_easy_setopt(easy_handle, CURLOPT_VERBOSE, 1L);

  // SET CURL_EASY CALLBACK //
  curl_easy_setopt(easy_handle, CURLOPT_WRITEFUNCTION, api_response_write_callback);
  curl_easy_setopt(easy_handle, CURLOPT_WRITEDATA, buffer->response);

  CURLcode response = curl_easy_perform(easy_handle);
  if (CURLE_OK != response){
    fprintf(stderr, "\n%s\n\n", curl_share_strerror(response));
    exit(EXIT_FAILURE);
  }

  //UNCOMMENT TO SEE JSON RESPONSE
  //fprintf(stderr,"\n\n%s\n\n", buffer->response);

  curl_slist_free_all(header);
}
