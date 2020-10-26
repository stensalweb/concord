#include <stdio.h>
#include <stdlib.h>

#include "logger.h"

void
__logger_assert(const char *expr_str, int expr, const char *file, int line, const char *func, const char *msg)
{
  if (!expr){
    fprintf(stderr,"[%s:%d]%s()\n\tASSERT FAILED:\t%s\n\tEXPECTED:\t%s\n",
      file, line, func, msg, expr_str);
    abort();
  }
}

