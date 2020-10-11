#include <stdio.h>
#include <stdlib.h>

#include "libconcord.h"

/* @todo instead of exit(), it should throw the error
    somewhere */
//this is redefined as a macro
void*
__concord_malloc(size_t size, unsigned long line)
{
  void *ptr = calloc(1, size);

  if (NULL == ptr){
    fprintf(stderr, "[%s:%lu] Out of memory(%lu bytes)\n",
              __FILE__, line, (unsigned long)size);
    exit(EXIT_FAILURE);
  }

  return ptr;
}

//this is redefined as a macro
void
__concord_free(void **p_ptr)
{
  if(NULL != p_ptr){
    free(*p_ptr);
    *p_ptr = NULL;
  } 
}

