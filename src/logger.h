#ifndef LIBCONCORD_LOGGER_H_
#define LIBCONCORD_LOGGER_H_

//#include <libconcord.h> implicit


#if CONCORD_DEBUG_MODE == 1
  void __logger_assert(const char * expr_str, int expr, const char *file, int line, const char *func, const char *msg);
  #define logger_assert(expr, msg) __logger_assert(#expr, expr, __FILE__, __LINE__, __func__, msg)

  #define logger_puts(msg) fprintf(stderr, "[%s:%d]%s()\n\t%s\n", __FILE__, __LINE__, __func__, msg)

  #define __logger_print_helper(fmt, ...) fprintf(stderr, "[%s:%d]%s()\n\t"fmt"\n%s",__FILE__, __LINE__, __func__,  __VA_ARGS__)
  #define logger_print(...) __logger_print_helper(__VA_ARGS__, "")
#else
  #include <assert.h>
  #define logger_assert(expr, msg) assert(expr)

  #define logger_puts(msg) 

  #define logger_print(...)
#endif
#endif
