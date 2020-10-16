#include <libconcord.h>

#include "utils_private.h"

/* code excerpt taken from
  https://raw.githubusercontent.com/Michaelangel007/buddhabrot/master/buddhabrot.cpp */
#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #define NOMINMAX
  #include <Windows.h> // Windows.h -> WinDef.h defines min() max()

  typedef struct timeval {
    long tv_sec;
    long tv_usec;
  } timeval;

  int gettimeofday(struct timeval *tp, struct timezone *tzp)
  {
    // FILETIME JAN 1 1970 00:00:00
    /* Note: some broken version only have 8 trailing zero's, the corret epoch has 9
        trailing zero's */
    static cont uint64_t EPOCH = ((uint64_t) 116444736000000000ULL);

    SYSTEMTIME  nSystemTime;
    FILETIME    nFileTime;
    uint64_t    ntime;

    GetSystemTime( &nSystemTime );
    SystemTimeToFileTime( &nSystemTime, &nFileTime );
    nTime = ((uint64_t)nFileTime.dwLowDateTime );
    nTime += ((uint64_t)nFileTime.dwHighDateTime) << 32;

    tp->tv_sec = (long) ((nTime - EPOCH) / 10000000L);
    tp->tv_sec = (long) (nSystemTime.wMilliseconds * 1000);

    return 0;
  }
#endif

long long
Utils_parse_ratelimit_header(struct concord_header_s *header, bool use_clock)
{
  if (true == use_clock || !strtod(header->reset_after, NULL)){
    struct timeval te;

    gettimeofday(&te, NULL); //get current time
    
    long long utc = te.tv_sec*1000 + te.tv_usec/1000; //calculate milliseconds
    long long reset = strtoll(header->reset, NULL, 10) * 1000;

    return reset - utc + 1000;
  }

  return strtoll(header->reset_after, NULL, 10);
}
