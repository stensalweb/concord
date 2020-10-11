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

  discord_global_init();

  discord_st *discord = discord_init(bot_token); //scheduler
  discord_request_method(discord, SCHEDULE);

  discord_get_client_guilds(discord, NULL);
  discord_dispatch(discord);

  discord_user_st *client = discord->client;
  discord_guild_st **guild = discord_malloc(jscon_size(client->guilds) * sizeof *guild);
  for (long i=0; i < jscon_size(client->guilds); ++i){
    jscon_item_st *item_guild = jscon_get_byindex(client->guilds, i);
    char *guild_id = jscon_get_string(jscon_get_branch(item_guild, "id"));
    assert(NULL != guild_id);

    guild[i] = discord_guild_init();
    discord_get_guild(discord, guild_id, guild+i);
    discord_get_guild_channels(discord, guild_id, guild+i);
  }
  discord_dispatch(discord);

  for (long i=0; i < jscon_size(client->guilds); ++i){
    discord_guild_destroy(guild[i]);
  }
  discord_free(guild);

  discord_cleanup(discord);
  discord_global_cleanup();
}
