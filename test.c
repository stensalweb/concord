#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "src/libdiscordc.h"
#include "src/global_config.h"

int main(void)
{
  FILE *f_bot_token = fopen("bot_token","rb");
  char bot_token[256];
  fgets(bot_token,255,f_bot_token);
  fclose(f_bot_token);

  discord_st *discord = discord_init(bot_token);

  discord_user_st *client = discord->client;

  discord_get_client_guilds(client);
  jsonc_item_st *first_guild = jsonc_get_byindex(client->guilds, 0);
  char *first_guild_id = jsonc_get_string(jsonc_get_branch(first_guild, "id"));
  assert(NULL != first_guild_id);

  discord_get_guild(discord->guild, first_guild_id);
  discord_get_channel(discord->channel, first_guild_id);

  discord_cleanup(discord);
  
  /*
  long response_code;
  curl_easy_getinfo(channel->easy_handle, CURLINFO_RESPONSE_CODE, &response_code);
  assert(200 == response_code);
  */
}
