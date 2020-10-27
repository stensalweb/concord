#include <string.h>

#include <libconcord.h>

#include "ratelimit.h"
#include "http_private.h"
#include "debug.h"
#include "hashtable.h"


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

struct concord_bucket_s*
Concord_bucket_init(char bucket_hash[])
{
  struct concord_bucket_s *new_bucket = safe_malloc(sizeof *new_bucket);

  new_bucket->num_conn = MAX_CONCURRENT_CONNS;
  new_bucket->queue = safe_malloc(sizeof *new_bucket->queue * new_bucket->num_conn);

  new_bucket->hash_key = strndup(bucket_hash, strlen(bucket_hash));
  debug_assert(NULL != new_bucket->hash_key, "Out of memory");

  return new_bucket;
}

/* @param ptr is NULL because we want to pass this function as a
    destructor callback to dictionary_set, which only accepts
    destructors with void* param */
void
Concord_bucket_destroy(void *ptr)
{
  struct concord_bucket_s *bucket = ptr;

  safe_free(bucket->queue);

  safe_free(bucket->hash_key);
  safe_free(bucket);
}

void
Concord_queue_recycle(concord_utils_st *utils, struct concord_bucket_s *bucket)
{
  debug_assert(NULL != bucket->queue[bucket->top], "Can't recycle empty slot");
  debug_assert(bucket->top < bucket->num_conn, "Queue top has reached threshold");

  ++bucket->top;
  ++utils->transfers_onhold;

  debug_print("Bucket top: %ld\n\tBucket size: %ld", bucket->top, bucket->num_conn);

  if (MAX_CONCURRENT_CONNS == utils->transfers_onhold){
    debug_puts("Reach max concurrent connections threshold, auto performing connections on hold ...");
    concord_dispatch((concord_st*)utils);
  }
}

/* push new connection to queue */
void
Concord_queue_push(concord_utils_st *utils, struct concord_bucket_s *bucket, struct concord_conn_s *conn)
{
  debug_assert(bucket->top < bucket->num_conn, "Queue top has reached threshold");

  bucket->queue[bucket->top] = conn; 
  conn->bucket = bucket;

  ++bucket->top;
  ++utils->transfers_onhold;

  debug_print("Bucket top: %ld\n\tBucket size: %ld", bucket->top, bucket->num_conn);

  if (MAX_CONCURRENT_CONNS == utils->transfers_onhold){
    debug_puts("Reach max concurrent connections threshold, auto performing connections on hold ...");
    concord_dispatch((concord_st*)utils);
  }
}

void
Concord_queue_pop(concord_utils_st *utils, struct concord_bucket_s *bucket)
{
  if (bucket->bottom == bucket->top) return; //nothing to pop

  struct concord_conn_s *conn = bucket->queue[bucket->bottom];
  debug_assert(NULL != conn, "Can't pop empty queue's slot");

  curl_multi_add_handle(utils->multi_handle, conn->easy_handle);

  ++bucket->bottom;
  --utils->transfers_onhold;

  debug_print("Bucket Bottom: %ld\n\tBucket top: %ld\n\tBucket size: %ld", bucket->bottom, bucket->top, bucket->num_conn);
}

void
Concord_client_buckets_append(concord_utils_st *utils, struct concord_bucket_s *bucket)
{
  ++utils->num_buckets;
  void *tmp = realloc(utils->client_buckets, sizeof *utils->client_buckets * utils->num_buckets);
  debug_assert(NULL != tmp, "Out of memory");

  utils->client_buckets = tmp;

  utils->client_buckets[utils->num_buckets-1] = bucket;
}

void
Concord_start_client_buckets(concord_utils_st *utils)
{
  for (size_t i=0; i < utils->num_buckets; ++i){
    Concord_queue_pop(utils, utils->client_buckets[i]);
    debug_print("Bucket Hash: %s\n\tBucket Size: %ld", utils->client_buckets[i]->hash_key, utils->client_buckets[i]->top);
  }
}

void
Concord_reset_client_buckets(concord_utils_st *utils)
{
  for (size_t i=0; i < utils->num_buckets; ++i){
    utils->client_buckets[i]->top = 0;
    utils->client_buckets[i]->bottom = 0;
  }
}

struct concord_bucket_s*
Concord_get_hashbucket(concord_utils_st *utils, char bucket_hash[])
{
  debug_assert(NULL != bucket_hash, "Bucket hash unspecified (NULL)");

  /* check if hashbucket with bucket_hash already exists */
  struct concord_bucket_s *bucket = dictionary_get(utils->bucket_dict, bucket_hash);

  if (NULL != bucket){
    debug_puts("Returning existing bucket");
    return bucket; //bucket exists return it
  }

  /* hashbucket doesn't exist, create it */
  bucket = Concord_bucket_init(bucket_hash);

  dictionary_set(utils->bucket_dict, bucket_hash, bucket, &Concord_bucket_destroy);

  Concord_client_buckets_append(utils, bucket);

  debug_puts("Returning new bucket");
  return bucket;
}
