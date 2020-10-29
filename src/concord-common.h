#ifndef LIBCONCORD_COMMON_H_
#define LIBCONCORD_COMMON_H_
//#include <libconcord.h> (implicit) 

enum http_method {
  NONE    = 0,
  DELETE  = 1,
  GET     = 2,
  POST    = 3,
  PATCH   = 4,
  PUT     = 5,
};

#define MAX_CONCURRENT_CONNS  15

/* ENDPOINTS */
#define CHANNELS           "/channels/%s"
#define CHANNELS_MESSAGES  "/channels/%s/messages"

#define GUILDS             "/guilds/%s"
#define GUILDS_CHANNELS    "/guilds/%s/channels"

#define USERS              "/users/%s"
#define USERS_GUILDS       "/users/%s/guilds"

/* memory.c */

void __safe_free(void **p_ptr);
#define safe_free(n) __safe_free((void**)&n)
void* __safe_malloc(size_t size, unsigned long line, char file[]);
#define safe_malloc(n) __safe_malloc(n, __LINE__, __FILE__)

/*************/
/* http.c */

/* 
  @param utils contains tools common to every request
  @param p_object is a pointer to the object to be loaded by load_cb
  @param load_cb is the function that will load the object attributes
    once a connection is completed
  @param http_method is the http method on the client side
    (GET, PUT, ...)
  @param endpoint is the format string that will be joined with the
    parameters to create a specific request
  @param __VAR_ARGS__ are the parameters that will be joined to the
    endpoint
*/
void Concord_http_request(
    concord_utils_st *utils,
    void **p_object,
    concord_load_obj_ft *load_cb,
    enum http_method http_method,
    char endpoint[], 
    ...);

/*************/
/* dispatch.c */

int Uv_start_timeout_cb(CURLM *multi_handle, long timeout_ms, void *p_userdata);
int Uv_handle_socket_cb(CURL *easy_handle, curl_socket_t sockfd, int action, void *p_userdata, void *p_socket);
void Concord_synchronous_perform(concord_utils_st *utils, struct concord_conn_s *conn);

/*************/
/* ratelimit.c */

char* Concord_tryget_major(char endpoint[]);
long long Concord_parse_ratelimit_header(struct dictionary_s *header, bool use_clock);

void Concord_queue_recycle(concord_utils_st *utils, struct concord_bucket_s *bucket);
void Concord_queue_push(concord_utils_st *utils, struct concord_bucket_s *bucket, struct concord_conn_s *conn);
void Concord_queue_pop(concord_utils_st *utils, struct concord_bucket_s *bucket);

void Concord_start_client_buckets(concord_utils_st *utils);
void Concord_stop_client_buckets(concord_utils_st *utils);
struct concord_bucket_s *Concord_get_hashbucket(concord_utils_st *utils, char bucket_hash[]);

void Concord_bucket_build(concord_utils_st *utils, void **p_object, concord_load_obj_ft *load_cb, enum http_method http_method, char bucket_key[], char url_route[]);

/*************/


#endif
