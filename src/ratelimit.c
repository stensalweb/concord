#include <string.h>
#include <ctype.h>

#include <libconcord.h>
#include "concord-common.h"

#include "hashtable.h"
#include "curl-ext.h"
#include "debug.h"


char*
Concord_tryget_major(char endpoint[])
{
  if (strstr(endpoint, CHANNELS)) return "channel_major";
  if (strstr(endpoint, GUILDS)) return "guild_major";
  //if (0 == strstr(endpoint, WEBHOOK)) return "webhook_major";
  return endpoint;
}

long long
Concord_parse_ratelimit_header(dictionary_st *header, bool use_clock)
{
  long long reset_after = dictionary_get_strtoll(header, "x-ratelimit-reset-after");

  if (true == use_clock || !reset_after){
    uv_timeval64_t te;

    uv_gettimeofday(&te); //get current time
    
    long long utc = te.tv_sec*1000 + te.tv_usec/1000; //calculate milliseconds
    long long reset = dictionary_get_strtoll(header, "x-ratelimit-reset") * 1000;
    long long delay_ms = reset - utc;

    if (delay_ms < 0){
      delay_ms = 0;
    }

    return delay_ms;
  }

  return reset_after;
}

static void
_concord_client_buckets_append(concord_utils_st *utils, struct concord_bucket_s *bucket)
{
  ++utils->num_buckets;
  void *tmp = realloc(utils->client_buckets, sizeof *utils->client_buckets * utils->num_buckets);
  DEBUG_ASSERT(NULL != tmp, "Out of memory");

  utils->client_buckets = tmp;

  utils->client_buckets[utils->num_buckets-1] = bucket;
}

/* @param ptr is NULL because we want to pass this function as a
    destructor callback to dictionary_set, which only accepts
    destructors with void* param */
static void
_concord_bucket_destroy(void *ptr)
{
  struct concord_bucket_s *bucket = ptr;

  safe_free(bucket->queue);

  safe_free(bucket->hash_key);
  safe_free(bucket);
}

static struct concord_bucket_s*
_concord_bucket_init(concord_utils_st *utils, char bucket_hash[])
{
  struct concord_bucket_s *new_bucket = safe_malloc(sizeof *new_bucket);

  new_bucket->num_conn = MAX_CONCURRENT_CONNS;
  new_bucket->queue = safe_malloc(sizeof *new_bucket->queue * new_bucket->num_conn);

  new_bucket->hash_key = strndup(bucket_hash, strlen(bucket_hash));
  DEBUG_ASSERT(NULL != new_bucket->hash_key, "Out of memory");

  int uvcode = uv_timer_init(utils->loop, &new_bucket->timer);
  DEBUG_ASSERT(!uvcode, uv_strerror(uvcode));

  new_bucket->p_utils = utils;
  new_bucket->timer.data = new_bucket;

  dictionary_set(utils->bucket_dict, bucket_hash, new_bucket, &_concord_bucket_destroy);

  _concord_client_buckets_append(utils, new_bucket);

  return new_bucket;
}

void
Concord_queue_recycle(concord_utils_st *utils, struct concord_bucket_s *bucket)
{
  DEBUG_ASSERT(NULL != bucket->queue[bucket->top], "Can't recycle empty slot");
  DEBUG_ASSERT(bucket->top < bucket->num_conn, "Queue top has reached threshold");

  ++bucket->top;
  ++utils->transfers_onhold;

  DEBUG_PRINT("Bucket top: %ld\n\tBucket size: %ld", bucket->top, bucket->num_conn);

  if (MAX_CONCURRENT_CONNS == utils->transfers_onhold){
    DEBUG_PUTS("Reach max concurrent connections threshold, auto performing connections on hold ...");
    concord_dispatch((concord_st*)utils);
  }
}

/* push new connection to queue */
void
Concord_queue_push(concord_utils_st *utils, struct concord_bucket_s *bucket, struct concord_conn_s *conn)
{
  DEBUG_ASSERT(bucket->top < bucket->num_conn, "Queue top has reached threshold");

  bucket->queue[bucket->top] = conn; 
  conn->p_bucket = bucket;

  ++bucket->top;
  ++utils->transfers_onhold;

  DEBUG_PRINT("Bucket top: %ld\n\tBucket size: %ld", bucket->top, bucket->num_conn);

  if (MAX_CONCURRENT_CONNS == utils->transfers_onhold){
    DEBUG_PUTS("Reach max concurrent connections threshold, auto performing connections on hold ...");
    concord_dispatch((concord_st*)utils);
  }
}

void
Concord_queue_pop(concord_utils_st *utils, struct concord_bucket_s *bucket)
{
  if (bucket->bottom == bucket->top) return; //nothing to pop

  struct concord_conn_s *conn = bucket->queue[bucket->bottom];
  DEBUG_ASSERT(NULL != conn, "Can't pop empty queue's slot");

  curl_multi_add_handle(utils->multi_handle, conn->easy_handle);

  ++bucket->bottom;
  --utils->transfers_onhold;

  DEBUG_PRINT("Bucket Bottom: %ld\n\tBucket top: %ld\n\tBucket size: %ld", bucket->bottom, bucket->top, bucket->num_conn);
}

void
Concord_start_client_buckets(concord_utils_st *utils)
{
  for (size_t i=0; i < utils->num_buckets; ++i){
    Concord_queue_pop(utils, utils->client_buckets[i]);
    DEBUG_PRINT("Bucket Hash: %s\n\tBucket Size: %ld", utils->client_buckets[i]->hash_key, utils->client_buckets[i]->top);
  }
}

void
Concord_stop_client_buckets(concord_utils_st *utils)
{
  for (size_t i=0; i < utils->num_buckets; ++i){
    utils->client_buckets[i]->top = 0;
    utils->client_buckets[i]->bottom = 0;
  }
}

struct concord_bucket_s*
Concord_get_hashbucket(concord_utils_st *utils, char bucket_hash[])
{
  DEBUG_ASSERT(NULL != bucket_hash, "Bucket hash unspecified (NULL)");

  /* check if hashbucket with bucket_hash already exists */
  struct concord_bucket_s *bucket = dictionary_get(utils->bucket_dict, bucket_hash);

  if (NULL != bucket){
    DEBUG_PUTS("Returning existing bucket");
    return bucket; //bucket exists return it
  }

  /* hashbucket doesn't exist, create it */
  bucket = _concord_bucket_init(utils, bucket_hash);

  DEBUG_PUTS("Returning new bucket");
  return bucket;
}

/* @param ptr is NULL because we want to pass this function as a
    destructor callback to dictionary_set, which only accepts
    destructors with void* param */
static void
_concord_conn_destroy(void *ptr)
{
  struct concord_conn_s *conn = ptr;

  curl_easy_cleanup(conn->easy_handle);
  safe_free(conn->response_body.str);
  safe_free(conn);
}

/* appends new connection node to the end of the list */
static struct concord_conn_s*
_concord_conn_init(concord_utils_st *utils, char bucket_key[])
{
  DEBUG_ASSERT(NULL != bucket_key, "Bucket key not specified (NULL)");

  struct concord_conn_s *new_conn = safe_malloc(sizeof *new_conn);

  new_conn->easy_handle = Curl_easy_default_init(utils, new_conn);

  char easy_key[18];
  sprintf(easy_key, "%p", new_conn->easy_handle);
  void *tmp = dictionary_set(utils->easy_dict, easy_key, new_conn, &_concord_conn_destroy);
  DEBUG_ASSERT(NULL != tmp, "Couldn't add new_conn to dictionary");

  return new_conn;
}

void
Concord_bucket_build(
  concord_utils_st *utils,
  void **p_object, 
  concord_load_obj_ft *load_cb,
  enum http_method http_method,
  char bucket_key[],
  char url_route[])
{
  struct concord_bucket_s *bucket = dictionary_get(utils->bucket_dict, bucket_key);
  DEBUG_PUTS(bucket_key);
  
  if (NULL == bucket){
    /* this is the first time using this bucket_keyr. We will perform a blocking
        connection to the Discord API, in order to link this bucket_key with a new
        or existing bucket */
    struct concord_conn_s *new_conn = _concord_conn_init(utils, bucket_key);
    DEBUG_ASSERT(NULL != new_conn, "Out of memory");
    DEBUG_PUTS("New conn created");

    Curl_set_method(new_conn, http_method); //set the http request method (GET, POST, ...)
    Curl_set_url(new_conn, url_route); //set the http request url

    new_conn->load_cb = load_cb; //callback that will perform changes to provided object
    new_conn->p_object = p_object; //save object for when load_cb is executed
    
    Concord_synchronous_perform(utils, new_conn); //execute a blocking connection to generate this bucket hash

    DEBUG_PUTS("Fetched conn matching hashbucket");

    char *bucket_hash = dictionary_get(utils->header, "x-ratelimit-bucket");
    DEBUG_PUTS(bucket_hash);

    bucket = Concord_get_hashbucket(utils, bucket_hash); //return created/found bucket matching bucket hash

    /* try to find a empty bucket queue slot */
    size_t i = bucket->top;
    while (NULL != bucket->queue[i]){
      ++i;
    }
    DEBUG_ASSERT(i < bucket->num_conn, "Queue top has reached threshold");

    bucket->queue[i] = new_conn; //append conn created to the end of the bucket queue

    new_conn->p_bucket = bucket; //reference bucket to the conn created

    void *tmp = dictionary_set(utils->bucket_dict, bucket_key, bucket, NULL); //link this bucket_key with created/found hashbucket
    DEBUG_ASSERT(tmp == bucket, "Couldn't link new bucket_key with existing bucket");
  }
  else {
    /* add connection to bucket or reuse innactive existing one */
    DEBUG_PRINT("Matching hashbucket found: %s", bucket->hash_key);
    DEBUG_ASSERT(bucket->top < bucket->num_conn, "Queue top has reached threshold");

    struct concord_conn_s *new_conn = bucket->queue[bucket->top];
    if (NULL == new_conn){
      DEBUG_PUTS("Bucket exists but needs a new conn pushed to it (can't recycle)");

      new_conn = _concord_conn_init(utils, bucket_key);
      DEBUG_ASSERT(NULL != new_conn, "Out of memory");

      Concord_queue_push(utils, bucket, new_conn);
    } else { 
      DEBUG_PUTS("Recycling existing connection");
      Concord_queue_recycle(utils, bucket);
    }

    Curl_set_method(new_conn, http_method);
    Curl_set_url(new_conn, url_route);

    new_conn->load_cb = load_cb;
    new_conn->p_object = p_object;
  }
}
