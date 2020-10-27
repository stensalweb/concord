#ifndef LIBCONCORD_DEBUG_H_
#define LIBCONCORD_DEBUG_H_

//#include <libconcord.h> implicit


#if CONCORD_DEBUG_MODE == 1
  void __debug_assert(const char * expr_str, int expr, const char *file, int line, const char *func, const char *msg);
  #define debug_assert(expr, msg) __debug_assert(#expr, expr, __FILE__, __LINE__, __func__, msg)

  #define debug_puts(msg) fprintf(stderr, "[%s:%d]%s()\n\t%s\n", __FILE__, __LINE__, __func__, msg)

  #define __debug_print_helper(fmt, ...) fprintf(stderr, "[%s:%d]%s()\n\t"fmt"\n%s",__FILE__, __LINE__, __func__,  __VA_ARGS__)
  #define debug_print(...) __debug_print_helper(__VA_ARGS__, "")
#else
  #include <assert.h>
  #define debug_assert(expr, msg) assert(expr)

  #define debug_puts(msg) 

  #define debug_print(...)
#endif
#endif
