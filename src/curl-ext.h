#ifndef LIBCONCORD_CURL_H_
#define LIBCONCORD_CURL_H_
//#include <libconcord.h> (implicit) 


struct curl_slist* Curl_request_header_init(concord_utils_st *utils);
size_t Curl_header_cb(char *content, size_t size, size_t nmemb, void *p_userdata);
size_t Curl_body_cb(char *content, size_t size, size_t nmemb, void *p_userdata);
CURL* Curl_easy_default_init(concord_utils_st *utils, struct concord_conn_s *conn);
CURLM* Curl_multi_default_init(concord_utils_st *utils);
void Curl_set_method(struct concord_conn_s *conn, enum http_method method);
void Curl_set_url(struct concord_conn_s *conn, char endpoint[]);

#endif
