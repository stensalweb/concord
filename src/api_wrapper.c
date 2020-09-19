#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <curl/curl.h>

#include "libdiscordc.h"
#include "global_config.h"

discord_st*
discord_init(char *bot_token)
{
  strcpy(g_config.bot_token, bot_token);

  curl_global_init(CURL_GLOBAL_DEFAULT);

  discord_st *new_discord = malloc(sizeof *new_discord);
  assert(NULL != new_discord);

  new_discord->channel = discord_channel_init();
  new_discord->guild = discord_guild_init();
  new_discord->user = discord_user_init();
  new_discord->client = discord_user_init();

  return new_discord;
}

void
discord_cleanup(discord_st *discord)
{
  discord_channel_destroy(discord->channel);
  discord_guild_destroy(discord->guild);
  discord_user_destroy(discord->user);
  discord_user_destroy(discord->client);

  free(discord);

  curl_global_cleanup();
}
