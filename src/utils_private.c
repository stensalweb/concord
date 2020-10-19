#include <libconcord.h>

#include "utils_private.h"
#include "hashtable.h"

#define DEFAULT_WAITMS 1000


long long
Utils_parse_ratelimit_header(dictionary_st *header, bool use_clock)
{
  long long reset_after = dictionary_get_strtoll(header, "x-ratelimit-reset-after");
  if (0 < reset_after) reset_after = DEFAULT_WAITMS;

  if (true == use_clock || !reset_after){
    uv_timeval64_t te;

    uv_gettimeofday(&te); //get current time
    
    long long utc = te.tv_sec*1000 + te.tv_usec/1000; //calculate milliseconds
    long long reset = dictionary_get_strtoll(header, "x-ratelimit-reset") * 1000;
    long long delay_ms = reset - utc;

    if (0 <= delay_ms){
      delay_ms = DEFAULT_WAITMS;
    }

    return delay_ms;
  }

  return reset_after;
}
