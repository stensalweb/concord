#ifndef LIBCONCORD_UTILS_H_
#define LIBCONCORD_UTILS_H_

//#include <libconcord.h> << implicit


#define Utils_print_debug(msg) fprintf(stderr, "[%s:%d]%s()\n\t%s\n", \
                                    __FILE__, __LINE__, __func__, msg)

void __Utils_assert(const char * expr_str, int expr, const char *file, int line, const char *func, const char *msg);
#define Utils_assert(expr, msg) \
      __Utils_assert(#expr, expr, __FILE__, __LINE__, __func__, msg)

char* Utils_tryget_major(char endpoint[]);
long long Utils_parse_ratelimit_header(struct dictionary_s *header, bool use_clock);

#endif
