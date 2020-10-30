#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <libconcord.h>
#include "concord-common.h"

#include "hashtable.h"
#include "debug.h"


/* @todo create distinction between bot and user token */
struct curl_slist*
Curl_request_header_init(concord_utils_st *utils)
{
  char auth[MAX_HEADER_LEN] = "Authorization: Bot "; 

  struct curl_slist *new_header = NULL;
  void *tmp;
  new_header = curl_slist_append(new_header,"X-RateLimit-Precision: millisecond");
  DEBUG_ASSERT(NULL != new_header, "Couldn't create request header");

  tmp = curl_slist_append(new_header, strcat(auth, utils->token));
  DEBUG_ASSERT(NULL != tmp, "Couldn't create request header");

  tmp = curl_slist_append(new_header,"User-Agent: concord (http://github.com/LucasMull/concord, v0.0)");
  DEBUG_ASSERT(NULL != tmp, "Couldn't create request header");

  tmp = curl_slist_append(new_header,"Content-Type: application/json");
  DEBUG_ASSERT(NULL != tmp, "Couldn't create request header");

  return new_header;
}

/* this is a very crude http header parser, splits key/value pairs at ':' */
size_t
Curl_header_cb(char *content, size_t size, size_t nmemb, void *p_userdata)
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
  DEBUG_ASSERT(NULL != field, "Out of memory");

  /* store key/value pair in a dictionary */
  void *tmp = dictionary_set(header, key, field, &free);
  DEBUG_ASSERT(tmp == field, "Couldn't fetch header content");

  return realsize; /* return value for curl internals */
}

/* get curl response body */
size_t
Curl_body_cb(char *content, size_t size, size_t nmemb, void *p_userdata)
{
  size_t realsize = size * nmemb;
  struct concord_response_s *response_body = p_userdata;

  char *tmp = realloc(response_body->str, response_body->size + realsize + 1);
  DEBUG_ASSERT(NULL != tmp, "Out of memory");

  response_body->str = tmp;
  memcpy(response_body->str + response_body->size, content, realsize);
  response_body->size += realsize;
  response_body->str[response_body->size] = '\0';

  return realsize;
}


/* init easy handle with some default opt */
CURL*
Curl_easy_default_init(concord_utils_st *utils, struct concord_conn_s *conn)
{
  CURL *new_easy_handle = curl_easy_init();
  DEBUG_ASSERT(NULL != new_easy_handle, "Out of memory");

  CURLcode ecode;
  /*
  DEBUG_EXEC( ecode = curl_easy_setopt(new_easy_handle, CURLOPT_VERBOSE, 2L) );
  DEBUG_EXEC( DEBUG_ASSERT(CURLE_OK == ecode, curl_easy_strerror(ecode)) );
  */
  ecode = curl_easy_setopt(new_easy_handle, CURLOPT_HTTPHEADER, utils->request_header);
  DEBUG_ASSERT(CURLE_OK == ecode, curl_easy_strerror(ecode));

  ecode = curl_easy_setopt(new_easy_handle, CURLOPT_FOLLOWLOCATION, 1L);
  DEBUG_ASSERT(CURLE_OK == ecode, curl_easy_strerror(ecode));

  ecode = curl_easy_setopt(new_easy_handle, CURLOPT_PRIVATE, conn);
  DEBUG_ASSERT(CURLE_OK == ecode, curl_easy_strerror(ecode));

  // SET CURL_EASY CALLBACKS //
  ecode = curl_easy_setopt(new_easy_handle, CURLOPT_WRITEFUNCTION, &Curl_body_cb);
  DEBUG_ASSERT(CURLE_OK == ecode, curl_easy_strerror(ecode));

  ecode = curl_easy_setopt(new_easy_handle, CURLOPT_WRITEDATA, &conn->response_body);
  DEBUG_ASSERT(CURLE_OK == ecode, curl_easy_strerror(ecode));

  ecode = curl_easy_setopt(new_easy_handle, CURLOPT_HEADERFUNCTION, &Curl_header_cb);
  DEBUG_ASSERT(CURLE_OK == ecode, curl_easy_strerror(ecode));

  ecode = curl_easy_setopt(new_easy_handle, CURLOPT_HEADERDATA, utils->header);
  DEBUG_ASSERT(CURLE_OK == ecode, curl_easy_strerror(ecode));

  return new_easy_handle;
}

/* init multi handle with some default opt */
CURLM*
Curl_multi_default_init(concord_utils_st *utils)
{
  CURLM *new_multi_handle = curl_multi_init();
  DEBUG_ASSERT(NULL != new_multi_handle, "Out of memory");

  CURLMcode mcode;
  mcode = curl_multi_setopt(new_multi_handle, CURLMOPT_SOCKETFUNCTION, &Curl_handle_socket_cb);
  DEBUG_ASSERT(CURLM_OK == mcode, curl_multi_strerror(mcode));

  mcode = curl_multi_setopt(new_multi_handle, CURLMOPT_SOCKETDATA, utils);
  DEBUG_ASSERT(CURLM_OK == mcode, curl_multi_strerror(mcode));

  mcode = curl_multi_setopt(new_multi_handle, CURLMOPT_TIMERFUNCTION, &Curl_start_timeout_cb);
  DEBUG_ASSERT(CURLM_OK == mcode, curl_multi_strerror(mcode));

  mcode = curl_multi_setopt(new_multi_handle, CURLMOPT_TIMERDATA, &utils->timeout);
  DEBUG_ASSERT(CURLM_OK == mcode, curl_multi_strerror(mcode));

  return new_multi_handle;
}

void
Curl_set_method(struct concord_conn_s *conn, enum http_method method)
{
  CURLcode ecode;
  switch (method){
  case DELETE:
      ecode = curl_easy_setopt(conn->easy_handle, CURLOPT_CUSTOMREQUEST, "DELETE");
      DEBUG_ASSERT(CURLE_OK == ecode, curl_easy_strerror(ecode));
      return;
  case GET:
      ecode = curl_easy_setopt(conn->easy_handle, CURLOPT_HTTPGET, 1L);
      DEBUG_ASSERT(CURLE_OK == ecode, curl_easy_strerror(ecode));
      return;
  case POST:
      ecode = curl_easy_setopt(conn->easy_handle, CURLOPT_POST, 1L);
      DEBUG_ASSERT(CURLE_OK == ecode, curl_easy_strerror(ecode));
      return;
  case PATCH:
      ecode = curl_easy_setopt(conn->easy_handle, CURLOPT_CUSTOMREQUEST, "PATCH");
      DEBUG_ASSERT(CURLE_OK == ecode, curl_easy_strerror(ecode));
      return;
  case PUT:
      ecode = curl_easy_setopt(conn->easy_handle, CURLOPT_UPLOAD, 1L);
      DEBUG_ASSERT(CURLE_OK == ecode, curl_easy_strerror(ecode));
      return;
  default:
      DEBUG_PRINT("Unknown http_method\n\tCode: %d", method);
      abort();
  }
}

void
Curl_set_url(struct concord_conn_s *conn, char endpoint[])
{
  char base_url[MAX_URL_LEN] = BASE_URL;
  CURLcode ecode = curl_easy_setopt(conn->easy_handle, CURLOPT_URL, strcat(base_url, endpoint));
  DEBUG_ASSERT(CURLE_OK == ecode, curl_easy_strerror(ecode));
}
