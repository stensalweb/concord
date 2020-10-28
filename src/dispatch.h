#ifndef LIBCONCORD_DISPATCH_H_
#define LIBCONCORD_DISPATCH_H_
//#include <libconcord.h> (implicit)

int Curl_start_timeout_cb(CURLM *multi_handle, long timeout_ms, void *p_userdata);
int Curl_handle_socket_cb(CURL *easy_handle, curl_socket_t sockfd, int action, void *p_userdata, void *p_socket);

#endif
