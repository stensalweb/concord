#include <libconcord.h>

#include "utils_private.h"

long long
Utils_parse_ratelimit_header(struct concord_header_s *header, bool use_clock)
{
  if (true == use_clock || !strtod(header->reset_after, NULL)){
    uv_timeval64_t te;

    uv_gettimeofday(&te); //get current time
    
    long long utc = te.tv_sec*1000 + te.tv_usec/1000; //calculate milliseconds
    long long reset = strtoll(header->reset, NULL, 10) * 1000;

    return reset - utc;
  }

  return strtoll(header->reset_after, NULL, 10);
}
