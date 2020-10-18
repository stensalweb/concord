#ifndef LIBCONCORD_API_WRAPPER_H_
#define LIBCONCORD_API_WRAPPER_H_
//#include <libconcord.h> (implicit) 

enum http_method {
  NONE   = 0,
  DELETE = 1,
  GET    = 2,
  PATCH  = 3,
  POST   = 4,
  PUT    = 5,
};

void HTTP_GET(concord_utils_st *utils, struct concord_clist_s *conn_list, char endpoint[]);
void HTTP_POST(concord_utils_st *utils, struct concord_clist_s *conn_list, char endpoint[]);

void Concord_http_request(concord_utils_st *utils, void **p_object, char conn_key[], char endpoint[], concord_ld_object_ft *load_cb, enum http_method http_method);

#endif
