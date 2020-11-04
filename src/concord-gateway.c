#include <stdio.h>
#include <stdlib.h>

#include <libconcord.h>
#include "concord-common.h"

#include "hashtable.h"
#include "debug.h"


concord_gateway_st*
Concord_gateway_init()
{
  concord_gateway_st *new_gateway = safe_malloc(sizeof *new_gateway);
  new_gateway->easy_handle = cws_new(BASE_GATEWAY_URL, NULL, NULL);
  DEBUG_ASSERT(NULL != new_gateway->easy_handle, "Out of memory");

  return new_gateway;
}

void
Concord_gateway_destroy(concord_gateway_st *gateway)
{
  cws_free(gateway->easy_handle);

  safe_free(gateway); 
}

