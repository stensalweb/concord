#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "libconcord.h"

int main(void)
{
  FILE *f_bot_token = fopen("bot_token","rb");
  char bot_token[BOT_TOKEN_LENGTH];
  fgets(bot_token,BOT_TOKEN_LENGTH-1,f_bot_token);
  fclose(f_bot_token);

  concord_global_init();

  concord_st *concord = concord_init(bot_token); //scheduler
  concord_request_method(concord, SCHEDULE);

  concord_get_client_guilds(concord, NULL);
  concord_dispatch(concord);

  concord_user_st *client = concord->client;
  concord_guild_st **guild = concord_malloc(jscon_size(client->guilds) * sizeof *guild);
  for (long i=0; i < jscon_size(client->guilds); ++i){
    jscon_item_st *item_guild = jscon_get_byindex(client->guilds, i);
    char *guild_id = jscon_get_string(jscon_get_branch(item_guild, "id"));
    assert(NULL != guild_id);

    guild[i] = concord_guild_init();
    concord_get_guild(concord, guild_id, guild+i);
    concord_get_guild_channels(concord, guild_id, guild+i);
  }
  concord_dispatch(concord);

  for (long i=0; i < jscon_size(client->guilds); ++i){
    concord_guild_destroy(guild[i]);
  }
  concord_free(guild);

  concord_cleanup(concord);
  concord_global_cleanup();
}
