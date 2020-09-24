#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "src/libdiscordc.h"

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

  jsonc_item_st *first_guild = jsonc_get_byindex(client->guilds, 0);
  char *first_guild_id = jsonc_get_string(jsonc_get_branch(first_guild, "id"));
  assert(NULL != first_guild_id);

  discord_get_guild(discord, first_guild_id);
  discord_get_channel(discord, first_guild_id);

  discord_cleanup(discord);
  
  /*
  long response_code;
  curl_easy_getinfo(channel->easy_handle, CURLINFO_RESPONSE_CODE, &response_code);
  assert(200 == response_code);
  */
}
