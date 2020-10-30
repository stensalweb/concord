#ifndef LIBCONCORD_COMMON_H_
#define LIBCONCORD_COMMON_H_
//#include <libconcord.h> (implicit) 

#define BASE_URL "https://discord.com/api"

#define MAX_CONCURRENT_CONNS  15

#define BUCKET_DICTIONARY_SIZE  30
#define CONN_DICTIONARY_SIZE    100
#define HEADER_DICTIONARY_SIZE  15

enum discord_limits {
  MAX_NAME_LEN           = 100,
  MAX_TOPIC_LEN          = 1024,
  MAX_DESCRIPTION_LEN    = 1024,
  MAX_USERNAME_LEN       = 32,
  MAX_DISCRIMINATOR_LEN  = 4,
  MAX_HASH_LEN           = 1024,
  MAX_LOCALE_LEN         = 15,
  MAX_EMAIL_LEN          = 254,
  MAX_REGION_LEN         = 15,
  MAX_HEADER_LEN         = 512,
  MAX_URL_LEN            = 512,
};

/* HTTP RESPONSE CODES
https://discord.com/developers/docs/topics/opcodes-and-status-codes#http-http-response-codes */
enum discord_http_code {
  CURL_NO_RESPONSE              = 0,
  DISCORD_OK                    = 200,
  DISCORD_CREATED               = 201,
  DISCORD_NO_CONTENT            = 204,
  DISCORD_NOT_MODIFIED          = 304,
  DISCORD_BAD_REQUEST           = 400,
  DISCORD_UNAUTHORIZED          = 401,
  DISCORD_FORBIDDEN             = 403,
  DISCORD_NOT_FOUND             = 404,
  DISCORD_METHOD_NOT_ALLOWED    = 405,
  DISCORD_TOO_MANY_REQUESTS     = 429,
  DISCORD_GATEWAY_UNAVAILABLE   = 502,
};

/* SNOWFLAKES
https://discord.com/developers/docs/reference#snowflakes */
enum discord_snowflake {
  SNOWFLAKE_INCREMENT           = 12,
  SNOWFLAKE_PROCESS_ID          = 17,
  SNOWFLAKE_INTERNAL_WORKER_ID  = 22,
  SNOWFLAKE_TIMESTAMP           = 64,
};

/* ENDPOINTS */
#define CHANNELS           "/channels/%s"
#define CHANNELS_MESSAGES  CHANNELS"/messages"

#define GUILDS             "/guilds/%s"
#define GUILDS_CHANNELS    GUILDS"/channels"

#define USERS              "/users/%s"
#define USERS_GUILDS       USERS"/guilds"


struct concord_context_s {
  uv_poll_t poll_handle;
  curl_socket_t sockfd;
};

struct concord_response_s {
  char *str;
  size_t size;
};

typedef void (concord_load_obj_ft)(void **p_object, struct concord_response_s *response_body);

struct concord_conn_s {
  struct concord_context_s *context;
  CURL *easy_handle; //easy handle used to perform the request

  struct concord_response_s response_body; //stores response body associated with the easy_handle

  void **p_object; //hold onto object to be passed as a load_cb parameter
  concord_load_obj_ft *load_cb; //object load callback

  struct concord_bucket_s *p_bucket; //bucket this connection node is a part of
};
/*
struct concord_list_s {
  struct concord_conn_s *conn;
  struct concord_list_s *next;
};
*/
struct concord_bucket_s {
  char *hash_key;

  uv_timer_t timer;
  int remaining;
/*
  struct concord_list_s *active_conns;
  size_t num_active;
*/
  struct concord_conn_s **queue_conns;
  size_t num_queue;
  size_t bottom;
  size_t top;

  struct concord_utils_s *p_utils;
};

typedef struct concord_utils_s {
  char *token; /* @todo hash/unhash token */

  struct curl_slist *request_header; /* the default request header sent to discord servers */

  CURLM *multi_handle;
  int transfers_onhold;
  int transfers_running;

  uv_loop_t *loop;
  uv_timer_t timeout;

  struct concord_bucket_s **client_buckets;
  size_t num_buckets;

  struct dictionary_s *bucket_dict; //get buckets by their endpoints/major parameters
  struct dictionary_s *header; /* holds the http response header */
} concord_utils_st;


/* memory.c */

void __safe_free(void **p_ptr);
#define safe_free(n) __safe_free((void**)&n)
void* __safe_malloc(size_t size, unsigned long line, char file[]);
#define safe_malloc(n) __safe_malloc(n, __LINE__, __FILE__)

/*************/
/* concord-common.c */

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
    concord_st *concord,
    void **p_object,
    concord_load_obj_ft *load_cb,
    enum http_method http_method,
    char endpoint[], 
    ...);

/*************/
/* concord-dispatch.c */


int Curl_start_timeout_cb(CURLM *multi_handle, long timeout_ms, void *p_userdata);
int Curl_handle_socket_cb(CURL *easy_handle, curl_socket_t sockfd, int action, void *p_userdata, void *p_socket);
void Concord_synchronous_perform(concord_utils_st *utils, struct concord_conn_s *conn);

/*************/
/* concord-ratelimit.c */


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
/* curl-ext.c */

size_t Curl_header_cb(char *content, size_t size, size_t nmemb, void *p_userdata);
size_t Curl_body_cb(char *content, size_t size, size_t nmemb, void *p_userdata);

struct curl_slist* Curl_request_header_init(concord_utils_st *utils);
CURL* Curl_easy_default_init(concord_utils_st *utils, struct concord_conn_s *conn);
CURLM* Curl_multi_default_init(concord_utils_st *utils);

void Curl_set_method(struct concord_conn_s *conn, enum http_method method);
void Curl_set_url(struct concord_conn_s *conn, char endpoint[]);


#endif
