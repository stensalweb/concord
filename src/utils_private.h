#ifndef LIBCONCORD_UTILS_H_
#define LIBCONCORD_UTILS_H_

//#include <libconcord.h> << implicit

#ifdef _WIN32
  #define WAITMS(t) Sleep(t)
#else
  #include <sys/time.h>
  #include <unistd.h>

  #define WAITMS(t) usleep((t)*1000)
#endif

struct concord_header_s;

long long Utils_parse_ratelimit_header(struct concord_header_s *header, bool use_clock);

#endif
