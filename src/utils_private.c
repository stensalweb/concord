#include <libconcord.h>

#include "utils_private.h"
#include "hashtable.h"

long long
Utils_parse_ratelimit_header(dictionary_st *header, bool use_clock)
{
  if (true == use_clock || !strtod(dictionary_get(header, XRL_RESET_AFTER), NULL)){
    uv_timeval64_t te;

    uv_gettimeofday(&te); //get current time
    
    long long utc = te.tv_sec*1000 + te.tv_usec/1000; //calculate milliseconds
    long long reset = strtoll(dictionary_get(header, XRL_RESET), NULL, 10) * 1000;

    return reset - utc;
  }

  return strtoll(dictionary_get(header,XRL_RESET_AFTER), NULL, 10);
}
