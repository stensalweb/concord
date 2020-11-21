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
Curl_request_header_init(concord_http_t *http)
{
  char auth[MAX_HEADER_LEN] = "Authorization: Bot "; 

  struct curl_slist *new_header = NULL;
  void *tmp; /* for checking potential errors */


  new_header = curl_slist_append(new_header,"X-RateLimit-Precision: millisecond");
  DEBUG_ASSERT(NULL != new_header, "Couldn't create request header");

  tmp = curl_slist_append(new_header, strcat(auth, http->token));
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
    return realsize; /* couldn't find key/value pair */

  *ptr = '\0'; /* isolate key from value at ':' */
  
  char *key = content;

  if ( NULL == (ptr = strstr(ptr+1, "\r\n")) )
    return realsize; /* couldn't find CRLF */

  *ptr = '\0'; /* remove CRLF from value */

  /* trim space from start of value string if necessary */
  int i=1; /* start from one position after ':' char */
  for ( ; isspace(content[strlen(content)+i]) ; ++i)
    continue;

  char *field = strdup(&content[strlen(content)+i]);
  DEBUG_ASSERT(NULL != field, "Out of memory");

  /* store key/value pair in a dictionary */
  void *res = dictionary_set(header, key, field, &free);
  DEBUG_ASSERT(res == field, "Couldn't fetch header content");

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
Concord_conn_easy_init(concord_http_t *http, struct concord_conn_s *conn)
{
  CURL *new_easy_handle = curl_easy_init();
  DEBUG_ASSERT(NULL != new_easy_handle, "Out of memory");

  CURLcode ecode;
  /*
  DEBUG_ONLY( ecode = curl_easy_setopt(new_easy_handle, CURLOPT_VERBOSE, 2L) );
  DEBUG_ONLY( DEBUG_ASSERT(CURLE_OK == ecode, curl_easy_strerror(ecode)) );
  */
  ecode = curl_easy_setopt(new_easy_handle, CURLOPT_HTTPHEADER, http->request_header);
  DEBUG_ASSERT(CURLE_OK == ecode, curl_easy_strerror(ecode));

  ecode = curl_easy_setopt(new_easy_handle, CURLOPT_FOLLOWLOCATION, 1L);
  DEBUG_ASSERT(CURLE_OK == ecode, curl_easy_strerror(ecode));

  ecode = curl_easy_setopt(new_easy_handle, CURLOPT_PRIVATE, conn);
  DEBUG_ASSERT(CURLE_OK == ecode, curl_easy_strerror(ecode));

  /* SET CURL_EASY CALLBACKS */
  ecode = curl_easy_setopt(new_easy_handle, CURLOPT_WRITEFUNCTION, &Curl_body_cb);
  DEBUG_ASSERT(CURLE_OK == ecode, curl_easy_strerror(ecode));

  ecode = curl_easy_setopt(new_easy_handle, CURLOPT_WRITEDATA, &conn->response_body);
  DEBUG_ASSERT(CURLE_OK == ecode, curl_easy_strerror(ecode));

  ecode = curl_easy_setopt(new_easy_handle, CURLOPT_HEADERFUNCTION, &Curl_header_cb);
  DEBUG_ASSERT(CURLE_OK == ecode, curl_easy_strerror(ecode));

  ecode = curl_easy_setopt(new_easy_handle, CURLOPT_HEADERDATA, http->header);
  DEBUG_ASSERT(CURLE_OK == ecode, curl_easy_strerror(ecode));

  return new_easy_handle;
}

/* init multi handle with some default opt */
CURLM*
Concord_http_multi_init(concord_http_t *http)
{
  CURLM *new_multi_handle = curl_multi_init();
  DEBUG_ASSERT(NULL != new_multi_handle, "Out of memory");

  CURLMcode mcode;
  mcode = curl_multi_setopt(new_multi_handle, CURLMOPT_SOCKETFUNCTION, &Concord_http_socket_cb);
  DEBUG_ASSERT(CURLM_OK == mcode, curl_multi_strerror(mcode));

  mcode = curl_multi_setopt(new_multi_handle, CURLMOPT_SOCKETDATA, http);
  DEBUG_ASSERT(CURLM_OK == mcode, curl_multi_strerror(mcode));

  mcode = curl_multi_setopt(new_multi_handle, CURLMOPT_TIMERFUNCTION, &Concord_http_timeout_cb);
  DEBUG_ASSERT(CURLM_OK == mcode, curl_multi_strerror(mcode));

  mcode = curl_multi_setopt(new_multi_handle, CURLMOPT_TIMERDATA, &http->timeout);
  DEBUG_ASSERT(CURLM_OK == mcode, curl_multi_strerror(mcode));

  return new_multi_handle;
}

/* init easy handle with some default opt */
CURL*
Concord_ws_easy_init(concord_ws_t *ws)
{
  /* missing on_binary, on_ping, on_pong */
  struct cws_callbacks cws_cbs = {
    .on_connect = &Concord_on_connect_cb,
    .on_text = &Concord_on_text_cb,
    .on_close = &Concord_on_close_cb,
    .data = ws,
  };

  CURL *new_easy_handle = cws_new(BASE_GATEWAY_URL, NULL, &cws_cbs);
  DEBUG_ASSERT(NULL != new_easy_handle, "Out of memory");

  CURLcode ecode;
  DEBUG_ONLY(ecode = curl_easy_setopt(new_easy_handle, CURLOPT_VERBOSE, 2L));
  DEBUG_ONLY_ASSERT(CURLE_OK == ecode, curl_easy_strerror(ecode));

  ecode = curl_easy_setopt(new_easy_handle, CURLOPT_FOLLOWLOCATION, 2L);
  DEBUG_ASSERT(CURLE_OK == ecode, curl_easy_strerror(ecode));

  return new_easy_handle;
}

/* init multi handle with some default opt */
CURLM*
Concord_ws_multi_init(concord_ws_t *ws)
{
  CURLM *new_multi_handle = curl_multi_init();
  DEBUG_ASSERT(NULL != new_multi_handle, "Out of memory");

  CURLMcode mcode;
  mcode = curl_multi_setopt(new_multi_handle, CURLMOPT_SOCKETFUNCTION, &Concord_ws_socket_cb);
  DEBUG_ASSERT(CURLM_OK == mcode, curl_multi_strerror(mcode));

  mcode = curl_multi_setopt(new_multi_handle, CURLMOPT_SOCKETDATA, ws);
  DEBUG_ASSERT(CURLM_OK == mcode, curl_multi_strerror(mcode));

  mcode = curl_multi_setopt(new_multi_handle, CURLMOPT_TIMERFUNCTION, &Concord_ws_timeout_cb);
  DEBUG_ASSERT(CURLM_OK == mcode, curl_multi_strerror(mcode));

  mcode = curl_multi_setopt(new_multi_handle, CURLMOPT_TIMERDATA, &ws->timeout);
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
      break;
  case GET:
      ecode = curl_easy_setopt(conn->easy_handle, CURLOPT_HTTPGET, 1L);
      break;
  case POST:
      ecode = curl_easy_setopt(conn->easy_handle, CURLOPT_POST, 1L);
      break;
  case PATCH:
      ecode = curl_easy_setopt(conn->easy_handle, CURLOPT_CUSTOMREQUEST, "PATCH");
      break;
  case PUT:
      ecode = curl_easy_setopt(conn->easy_handle, CURLOPT_UPLOAD, 1L);
      break;
  default:
      DEBUG_ERR("Unknown http_method\n\tCode: %d", method);
  }

  DEBUG_ASSERT(CURLE_OK == ecode, curl_easy_strerror(ecode));
}

void
Curl_set_url(struct concord_conn_s *conn, char endpoint[])
{
  char base_url[MAX_URL_LEN] = BASE_API_URL;


  CURLcode ecode = curl_easy_setopt(conn->easy_handle, CURLOPT_URL, strcat(base_url, endpoint));
  DEBUG_ASSERT(CURLE_OK == ecode, curl_easy_strerror(ecode));
}
