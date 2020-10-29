#ifndef LIBCONCORD_DEBUG_H_
#define LIBCONCORD_DEBUG_H_

//#include <libconcord.h> implicit


#if CONCORD_DEBUG_MODE == 1 /*DEBUG MODE ACTIVATED */
  /* @param msg string to be printed in debug mode */
  #define DEBUG_PUTS(msg) fprintf(stderr, "[%s:%d]%s()\n\t%s\n", __FILE__, __LINE__, __func__, msg)

  /* @param fmt like printf
     @param ... arguments to be parsed into fmt */
  #define __DEBUG_PRINT_HELPER(fmt, ...) fprintf(stderr, "[%s:%d]%s()\n\t"fmt"\n%s",__FILE__, __LINE__, __func__,  __VA_ARGS__)
  #define DEBUG_PRINT(...) __DEBUG_PRINT_HELPER(__VA_ARGS__, "")

  /* @param expr to be checked for its validity
     @param msg to be printed in case of invalidity */
  #define DEBUG_ASSERT(expr, msg) do { \
      if (!(expr)){ \
        DEBUG_PRINT("ASSERT FAILED:\t%s\n\tEXPECTED:\t%s", msg, #expr); \
        abort(); \
      } \
  } while(0)

  /* @param argument to be performed if debug mode is active */
  #define DEBUG_ONLY_ARG(arg) (arg)

#else /* NO EFFECT FROM ANY PREPROCESSOR DIRECTIVES */

  #define DEBUG_PUTS(msg) 

  #define DEBUG_PRINT(...)

  #include <assert.h>
  #define DEBUG_ASSERT(expr, msg) assert(expr)
  #define DEBUG_ONLY_ARG(arg)

#endif

#endif
