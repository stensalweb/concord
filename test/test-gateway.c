#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "libconcord.h"

int main(void)
{
  FILE *f_bot_token = fopen("bot_token","rb");
  assert(NULL != f_bot_token);

  char bot_token[100];
  fgets(bot_token, 99, f_bot_token);
  fclose(f_bot_token);

  concord_global_init();

  concord_st *concord = concord_init(bot_token);

  concord_gateway_connect(concord);

  uv_sleep(5000);
  //while (true == concord_gateway_isrunning(concord));

  concord_cleanup(concord);

  concord_global_cleanup();
}
