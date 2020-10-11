#ifndef LIBCONCORD_API_WRAPPER_H_
#define LIBCONCORD_API_WRAPPER_H_
//#include "libconcord.h" (implicit) 

void Concord_GET(concord_utils_st *utils, struct concord_clist_s *conn_list, char endpoint[]);
void Concord_POST(concord_utils_st *utils, struct concord_clist_s *conn_list, char endpoint[]);

struct concord_clist_s* Concord_get_conn(concord_utils_st *utils, char endpoint[], concord_load_ft *load_cb, curl_request_ft *request_cb);

void Concord_request_perform(concord_utils_st *utils, void **p_object, char endpoint[], concord_load_ft *load_cb, curl_request_ft *request_cb);

#endif
