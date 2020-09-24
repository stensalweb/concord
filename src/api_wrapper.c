#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <curl/curl.h>

#include "libdiscordc.h"


static void
set_discord_request_header(discord_utils_st *utils)
{
  char auth_header[MAX_HEADER_LENGTH] = "Authorization: Bot "; 

  struct curl_slist *new_header = NULL;
  new_header = curl_slist_append(new_header,"X-RateLimit-Precision: millisecond");
  assert(NULL != new_header);

  new_header = curl_slist_append(new_header,strcat(auth_header, utils->bot_token));
  assert(NULL != new_header);

  new_header = curl_slist_append(new_header,"User-Agent: DISCORDc (http://github.com/LucasMull/DISCORDc, v0.0)");
  assert(NULL != new_header);

  new_header = curl_slist_append(new_header,"Content-Type: application/json");
  assert(NULL != new_header);

  utils->header = new_header;
}

discord_st*
discord_init(char *bot_token)
{
  curl_global_init(CURL_GLOBAL_DEFAULT);

  discord_st *new_discord = malloc(sizeof *new_discord);
  assert(NULL != new_discord);

  /* TODO: create a utils init function */
  new_discord->utils = malloc(sizeof *new_discord->utils);
  assert(NULL != new_discord->utils);
  new_discord->utils->response = malloc(MAX_RESPONSE_LENGTH);
  assert(NULL != new_discord->utils);
  strncpy(new_discord->utils->bot_token, bot_token, 255);
  set_discord_request_header(new_discord->utils);


  new_discord->channel = discord_channel_init(new_discord->utils);
  new_discord->guild = discord_guild_init(new_discord->utils);
  new_discord->user = discord_user_init(new_discord->utils);
  new_discord->client = discord_user_init(new_discord->utils);

  return new_discord;
}

void
discord_cleanup(discord_st *discord)
{
  discord_channel_destroy(discord->channel);
  discord_guild_destroy(discord->guild);
  discord_user_destroy(discord->user);
  discord_user_destroy(discord->client);

  free(discord->utils->response);
  curl_slist_free_all(discord->utils->header);
  free(discord->utils);

  free(discord);

  curl_global_cleanup();
}
