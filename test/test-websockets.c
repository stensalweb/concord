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

    concord_t *concord = concord_init(bot_token);

    concord_ws_connect(concord);

    while (concord_ws_isrunning(concord))
        continue;

    concord_cleanup(concord);

    concord_global_cleanup();
}
