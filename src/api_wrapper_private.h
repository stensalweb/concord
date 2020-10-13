#ifndef LIBCONCORD_API_WRAPPER_H_
#define LIBCONCORD_API_WRAPPER_H_
//#include "libconcord.h" (implicit) 

#ifndef WAITMS
#ifdef _WIN32
#define WAITMS(t) Sleep(t)
#else 
#define WAITMS(t) usleep((t)*1000)
#endif
#endif

void Concord_GET(concord_utils_st *utils, struct concord_clist_s *conn_list, char endpoint[]);
void Concord_POST(concord_utils_st *utils, struct concord_clist_s *conn_list, char endpoint[]);

void Concord_perform_request(concord_utils_st *utils, void **p_object, char conn_key[], char endpoint[], concord_ld_object_ft *load_cb, curl_request_ft *request_cb);

#endif
