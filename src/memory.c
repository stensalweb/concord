#include <stdio.h>
#include <stdlib.h>

#include <libconcord.h>

/* @todo instead of exit(), it should throw the error
    somewhere */
//this is redefined as a macro
void*
__safe_malloc(size_t size, unsigned long line, char file[])
{
  void *ptr = calloc(1, size);

  if (NULL == ptr){
    fprintf(stderr, "[%s:%lu] Out of memory(%lu bytes)\n",
              file, line, (unsigned long)size);
    exit(EXIT_FAILURE);
  }

  return ptr;
}

//this is redefined as a macro
void
__safe_free(void **p_ptr)
{
  if(NULL != p_ptr){
    free(*p_ptr);
    *p_ptr = NULL;
  } 
}

