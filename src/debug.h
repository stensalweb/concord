#ifndef LIBCONCORD_DEBUG_H_
#define LIBCONCORD_DEBUG_H_

//#include <libconcord.h> implicit


#if CONCORD_DEBUG_MODE == 1 /* DEBUG MODE ACTIVE */
#       define DEBUG_OUT stderr
#       define DEBUG_FMT_PREFIX "[%s:%d] %s()\n\t"
#       define DEBUG_FMT_ARGS __FILE__, __LINE__, __func__
/* @param msg string to be printed in debug mode */
#       define DEBUG_PUTS(msg) fprintf(DEBUG_OUT, DEBUG_FMT_PREFIX "%s\n", DEBUG_FMT_ARGS, msg)
/* @param fmt like printf
   @param ... arguments to be parsed into fmt */
#       define __DEBUG_PRINT_HELPER(fmt, ...) fprintf(DEBUG_OUT, DEBUG_FMT_PREFIX fmt "\n%s", DEBUG_FMT_ARGS, __VA_ARGS__)
#       define DEBUG_PRINT(...) __DEBUG_PRINT_HELPER(__VA_ARGS__, "")
/* @param expr to be checked for its validity
   @param msg to be printed in case of invalidity */
#       define DEBUG_ASSERT(expr, msg) \
        do { \
            if (!(expr)){ \
              DEBUG_PRINT("Assert Failed:\t%s\n\tExpected:\t%s", msg, #expr); \
              abort(); \
            } \
        } while(0)
/* @param snippet to be executed if debug mode is active */
#       define DEBUG_ONLY(arg) (arg)
#else /* DEBUG MODE INNACTIVE */
#       define DEBUG_PUTS(msg) 
#       define DEBUG_PRINT(...)
/* DEBUG_ASSERT becomes a proxy for assert */
#       include <assert.h>
#       define DEBUG_ASSERT(expr, msg) assert(expr)
#       define DEBUG_ONLY(arg)
#endif

#endif
