#include <string.h>

#include <libconcord.h>

#include "http_private.h"
#include "utils.h"
#include "hashtable.h"


char*
Utils_tryget_major(char endpoint[])
{
  if (strstr(endpoint, CHANNELS)) return "channel_major";
  if (strstr(endpoint, GUILDS)) return "guild_major";
  //if (0 == strstr(endpoint, WEBHOOK)) return "webhook_major";
  return endpoint;
}

/* @todo if bucket hash is unknown, use reset_after timeout value */
long long
Utils_parse_ratelimit_header(dictionary_st *header, bool use_clock)
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
