#include <stdio.h>
#include <stdlib.h>

#include <libconcord.h>
#include "concord-common.h"

#include "debug.h"


/* @todo instead of exit(), it should throw the error
    somewhere */
//this is redefined as a macro
void*
__safe_malloc(size_t size, const char file[], const int line, const char func[])
{
  void *ptr = calloc(1, size);

  if (NULL == ptr){
    fprintf(stderr, "[%s:%d] %s()\n\tOut of memory(%ld bytes)\n", file, line, func, size);
    abort();
  }
#if CONCORD_MEMDEBUG_MODE == 1
  fprintf(stderr, "[%s:%d] %s()\n\tAlloc:\t%p(%ld bytes)\n", file, line, func, ptr, size);
#else
    (void)file;
    (void)line;
    (void)func;
#endif

  return ptr;
}

//this is redefined as a macro
void
__safe_free(void **p_ptr, const char file[], const int line, const char func[])
{
  if(*p_ptr){
    free(*p_ptr);
#if CONCORD_MEMDEBUG_MODE == 1
    fprintf(stderr, "[%s:%d] %s()\n\tFree:\t%p\n", file, line, func, *p_ptr);
#else
    (void)file;
    (void)line;
    (void)func;
#endif

    *p_ptr = NULL;
  } 
}

