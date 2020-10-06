#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "src/libconcord.h"

#define BOT_TOKEN_LENGTH 256

int main(void)
{
  FILE *f_bot_token = fopen("bot_token","rb");
  char bot_token[BOT_TOKEN_LENGTH];
  fgets(bot_token,BOT_TOKEN_LENGTH-1,f_bot_token);
  fclose(f_bot_token);

  discord_st *discord = discord_init(bot_token);

  discord_get_client_guilds(discord);
  discord_user_st *client = discord->client;

  for (size_t i=0; i < jscon_size(client->guilds); ++i){
    jscon_item_st *guild = jscon_get_byindex(client->guilds, i);
    char *guild_id = jscon_get_string(jscon_get_branch(guild, "id"));
    assert(NULL != guild_id);

    discord_get_guild(discord, guild_id);
    discord_get_guild_channels(discord, guild_id);
  }

  discord_cleanup(discord);
  
  /*
  long response_code;
  curl_easy_getinfo(channel->easy_handle, CURLINFO_RESPONSE_CODE, &response_code);
  assert(200 == response_code);
  */
}
