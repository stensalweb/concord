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

  discord_st *discord = discord_init(bot_token);
  discord_request_method(discord, ASYNC);

  discord_get_client_guilds(discord, NULL);
  
  discord_async_perform(discord);
  discord_user_st *client = discord->client;
  for (long i=0; i < jscon_size(client->guilds); ++i){
    jscon_item_st *guild = jscon_get_byindex(client->guilds, i);
    char *guild_id = jscon_get_string(jscon_get_branch(guild, "id"));
    assert(NULL != guild_id);

    discord_get_guild(discord, guild_id, NULL);
    discord_get_guild_channels(discord, guild_id, NULL);
  }
  discord_async_perform(discord);

  discord_cleanup(discord);
}
