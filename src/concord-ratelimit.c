#include <string.h>
#include <ctype.h>

#include <libconcord.h>
#include "concord-common.h"

#include "hashtable.h"
#include "debug.h"


char*
Concord_tryget_major(char endpoint[])
{
  if (strstr(endpoint, CHANNELS)) return "channel_major";
  if (strstr(endpoint, GUILDS)) return "guild_major";
  /* if (strstr(endpoint, WEBHOOK)) return "webhook_major"; */
  return endpoint;
}

long long
Concord_parse_ratelimit_header(struct concord_bucket_s *bucket, dictionary_st *header, bool use_clock)
{
  int remaining = dictionary_get_strtoll(header, "x-ratelimit-remaining");
  DEBUG_PRINT("Ratelimit remaining: %d", remaining);
  
  if (bucket){
    bucket->remaining = remaining;
  }

  if (remaining) return 0; /* no delay if remaining > 0 */


  long long reset_after = dictionary_get_strtoll(header, "x-ratelimit-reset-after");

  long long delay_ms;
  if (true == use_clock || !reset_after){
    uv_timeval64_t te;

    uv_gettimeofday(&te); /* get current time */
    
    long long utc = te.tv_sec*1000 + te.tv_usec/1000; /* calculate milliseconds */
    long long reset = dictionary_get_strtoll(header, "x-ratelimit-reset") * 1000;
    delay_ms = reset - utc;
    if (delay_ms < 0){
      delay_ms = 0;
    }
  } else {
    delay_ms = reset_after*1000;
  }

  return delay_ms;
}

static struct concord_conn_s*
_concord_conn_init(concord_http_st *http)
{
  struct concord_conn_s *new_conn = safe_malloc(sizeof *new_conn);

  new_conn->easy_handle = Concord_conn_easy_init(http, new_conn);

  return new_conn;
}

static void
_concord_conn_destroy(struct concord_conn_s *conn)
{
  if (conn->context){
    Concord_context_destroy(conn->context);
    conn->context = NULL;
  }

  curl_easy_cleanup(conn->easy_handle);
  safe_free(conn->response_body.str);
  safe_free(conn);
}

static void
_concord_client_buckets_append(concord_http_st *http, struct concord_bucket_s *bucket)
{
  ++http->num_buckets;

  void *tmp = realloc(http->client_buckets, sizeof *http->client_buckets * http->num_buckets);
  DEBUG_ASSERT(NULL != tmp, "Out of memory");

  http->client_buckets = tmp;

  http->client_buckets[http->num_buckets-1] = bucket;
}

/* @param ptr is NULL because we want to pass this function as a
    destructor callback to dictionary_set, which only accepts
    destructors with void* param */
static void
_concord_bucket_destroy(void *ptr)
{
  struct concord_bucket_s *bucket = ptr;

  for (size_t i=0; i < bucket->queue.size; ++i){
    if (bucket->queue.conns[i])
      _concord_conn_destroy(bucket->queue.conns[i]);
  }
  safe_free(bucket->queue.conns);

  safe_free(bucket->hash_key);

  safe_free(bucket);
}

static struct concord_bucket_s*
_concord_bucket_init(concord_http_st *http, char bucket_hash[])
{
  struct concord_bucket_s *new_bucket = safe_malloc(sizeof *new_bucket);

  new_bucket->queue.size = MAX_QUEUE_SIZE;
  new_bucket->queue.conns = safe_malloc(sizeof *new_bucket->queue.conns * new_bucket->queue.size);

  new_bucket->hash_key = strdup(bucket_hash);
  DEBUG_ASSERT(NULL != new_bucket->hash_key, "Out of memory");

  int uvcode = uv_timer_init(http->loop, &new_bucket->ratelimit_timer);
  DEBUG_ASSERT(!uvcode, uv_strerror(uvcode));

  new_bucket->p_http = http;
  uv_handle_set_data((uv_handle_t*)&new_bucket->ratelimit_timer, new_bucket);

  void *res = dictionary_set(http->bucket_dict, bucket_hash, new_bucket, &_concord_bucket_destroy);
  DEBUG_ASSERT(res == new_bucket, "Couldn't create new bucket");

  _concord_client_buckets_append(http, new_bucket);

  return new_bucket;
}

/* recycle existing innactive connection */
static void
_concord_queue_recycle(concord_http_st *http, struct concord_queue_s *queue)
{
  DEBUG_ASSERT(NULL != queue->conns[queue->top_onhold], "Can't recycle conn from a NULL queue slot");
  DEBUG_ASSERT(queue->top_onhold < queue->size, "Queue top has reached threshold");

  queue->conns[queue->top_onhold]->status = ON_HOLD;

  ++queue->top_onhold;
  ++http->transfers_onhold;

  if (queue->top_onhold == queue->size){
    DEBUG_PUTS("Reach queue threshold, auto performing ALL transfers on hold ...");
    Concord_transfers_run(http);
  }
}

/* push new connection to queue */
static void
_concord_queue_push(concord_http_st *http, struct concord_queue_s *queue, struct concord_conn_s *conn)
{
  DEBUG_ASSERT(NULL == queue->conns[queue->top_onhold], "Can't push conn to a non-NULL queue slot");
  DEBUG_ASSERT(queue->top_onhold < queue->size, "Queue top has reached threshold");

  conn->status = ON_HOLD;
  conn->p_bucket = (struct concord_bucket_s*)queue;

  queue->conns[queue->top_onhold] = conn; 

  ++queue->top_onhold;
  ++http->transfers_onhold;

  if (queue->top_onhold == queue->size){
    DEBUG_PUTS("Reach queue threshold, auto performing ALL transfers on hold ...");
    Concord_transfers_run(http);
  }
}

/* pops N connections from queue (essentially sets them for transfers) */
void
Concord_queue_npop(concord_http_st *http, struct concord_queue_s *queue, int num_conn)
{
  queue->bottom_running = queue->separator;

  if (queue->bottom_running == queue->top_onhold){
    DEBUG_PRINT("No conn left to be added\n\t" \
                "Bucket Hash:\t%s", 
                ((struct concord_bucket_s*)queue)->hash_key);
    return;
  }

  struct concord_conn_s *conn; 
  while (num_conn--){
    if (queue->separator == queue->top_onhold)
      return; /* no conn to pop */

    conn = queue->conns[queue->separator];
    DEBUG_ASSERT(NULL != conn, "Queue's slot is NULL, can't pop");

    curl_multi_add_handle(http->multi_handle, conn->easy_handle);
    conn->status = RUNNING;

    ++queue->separator;
    --http->transfers_onhold;
  }

  DEBUG_PRINT("Bucket Hash:\t%s\n\t" \
              "Queue Size:\t%ld\n\t" \
              "Queue Bottom:\t%ld\n\t" \
              "Queue Separator:%ld\n\t" \
              "Queue Top:\t%ld",
              ((struct concord_bucket_s*)queue)->hash_key,
              queue->size,
              queue->bottom_running,
              queue->separator,
              queue->top_onhold);
}

void
_concord_queue_reset(struct concord_queue_s *queue)
{
  queue->bottom_running = 0;
  queue->separator = 0;
  queue->top_onhold = 0;

  DEBUG_PRINT("Bucket Hash:\t%s\n\t" \
              "Queue Size:\t%ld\n\t" \
              "Queue Bottom:\t%ld\n\t" \
              "Queue Separator:%ld\n\t" \
              "Queue Top:\t%ld",
              ((struct concord_bucket_s*)queue)->hash_key,
              queue->size,
              queue->bottom_running,
              queue->separator,
              queue->top_onhold);
}

void
Concord_start_client_buckets(concord_http_st *http)
{
  struct concord_bucket_s **client_buckets = http->client_buckets;

  for (size_t i=0; i < http->num_buckets; ++i){
    Concord_queue_npop(http, &client_buckets[i]->queue, 1);
  }
}

void
Concord_stop_client_buckets(concord_http_st *http)
{
  struct concord_bucket_s **client_buckets = http->client_buckets;

  for (size_t i=0; i < http->num_buckets; ++i){
    _concord_queue_reset(&client_buckets[i]->queue);
  }
}

struct concord_bucket_s*
Concord_trycreate_bucket(concord_http_st *http, char bucket_hash[])
{
  DEBUG_ASSERT(NULL != bucket_hash, "Bucket hash unspecified (NULL)");

  /* check if hashbucket with bucket_hash already exists */
  struct concord_bucket_s *bucket = dictionary_get(http->bucket_dict, bucket_hash);
  if (!bucket){
    /* hashbucket doesn't exist, create it */
    DEBUG_PUTS("Bucket hash not found, creating new ...");
    bucket = _concord_bucket_init(http, bucket_hash);
  }

  return bucket;
}

void
Concord_bucket_build(
  concord_http_st *http,
  void **p_object, 
  concord_load_obj_ft *load_cb,
  enum http_method http_method,
  char bucket_key[],
  char url_route[])
{
  struct concord_bucket_s *bucket = dictionary_get(http->bucket_dict, bucket_key);
  DEBUG_PRINT("Bucket Key: %s", bucket_key);

  /* conn to be pushed to (or recycled from) bucket queue */
  struct concord_conn_s *new_conn;
  
  if (!bucket){ /* no bucket referencing the given bucket key */
    /* this is the first time using this bucket_key. We will perform a blocking
        connection to the Discord API, in order to fetch this bucket key
        corresponding bucket */
    new_conn = _concord_conn_init(http);
    DEBUG_ASSERT(NULL != new_conn, "Out of memory");
    DEBUG_PUTS("New conn created");


    Curl_set_method(new_conn, http_method); /* set the http request method (GET, POST, ...) */
    Curl_set_url(new_conn, url_route); /* set the http request url */

    new_conn->load_cb = load_cb; /* callback that will perform actions on object */
    new_conn->p_object = p_object; /* object to have action performed on */

    /* execute a synchronous connection to the API to fetch
        the bucket hash matching this bucket_key with a new
        or existing bucket

       the new_conn status is set to INNACTIVE, which means 
        it will be ready for recycling after it has performed
        this connection */
    Concord_register_bucket_key(http, new_conn, bucket_key);
  }
  else {
    /* found bucket reference from given key add connection
        to bucket or reuse innactive existing one */
    DEBUG_PRINT("Matching hashbucket found: %s", bucket->hash_key);
    DEBUG_ASSERT(bucket->queue.top_onhold < bucket->queue.size, "Queue top has reached threshold");

    new_conn = bucket->queue.conns[bucket->queue.top_onhold];
    if (!new_conn){
      DEBUG_PUTS("Bucket exists but needs a new conn pushed to it (can't recycle)");

      new_conn = _concord_conn_init(http);
      DEBUG_ASSERT(NULL != new_conn, "Out of memory");

      _concord_queue_push(http, &bucket->queue, new_conn);
    } else { 
      DEBUG_PUTS("Recycling existing connection");
      _concord_queue_recycle(http, &bucket->queue);
    }

    Curl_set_method(new_conn, http_method); /* set the http request method (GET, POST, ...) */
    Curl_set_url(new_conn, url_route); /* set the http request url */

    new_conn->load_cb = load_cb; /* callback that will perform actions on object */
    new_conn->p_object = p_object; /* object to have action performed on */
  }
}
