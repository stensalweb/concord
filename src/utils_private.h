#ifndef LIBCONCORD_UTILS_H_
#define LIBCONCORD_UTILS_H_

//#include <libconcord.h> << implicit

struct concord_header_s;

long long Utils_parse_ratelimit_header(struct concord_header_s *header, bool use_clock);

#endif
