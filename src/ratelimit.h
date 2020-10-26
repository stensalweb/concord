#ifndef LIBCONCORD_RATELIMIT_H_
#define LIBCONCORD_RATELIMIT_H_
//#include <libconcord.h> (implicit) 

char* Concord_tryget_major(char endpoint[]);
long long Concord_parse_ratelimit_header(struct dictionary_s *header, bool use_clock);

struct concord_bucket_s *Concord_bucket_init(char bucket_hash[]);
void Concord_bucket_destroy(void *ptr);
void Concord_queue_recycle(concord_utils_st *utils, struct concord_bucket_s *bucket);
void Concord_queue_push(concord_utils_st *utils, struct concord_bucket_s *bucket, struct concord_conn_s *conn);
void Concord_queue_pop(concord_utils_st *utils, struct concord_bucket_s *bucket);

void Concord_client_buckets_append(concord_utils_st *utils, struct concord_bucket_s *bucket);
void Concord_start_client_buckets(concord_utils_st *utils);
void Concord_reset_client_buckets(concord_utils_st *utils);
struct concord_bucket_s *Concord_get_hashbucket(concord_utils_st *utils, char bucket_hash[]);

#endif
