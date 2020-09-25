#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <assert.h>
#include <string.h>

#include "REST.h"
#include "libdiscordc.h"

discord_guild_st*
discord_guild_init(discord_utils_st* utils)
{
  discord_guild_st *new_guild = calloc(1, sizeof *new_guild);
  assert(NULL != new_guild);

  new_guild->id = calloc(1, SNOWFLAKE_INTERNAL_WORKER_ID);
  assert(NULL != new_guild->id);

  new_guild->name = calloc(1, NAME_LENGTH);
  assert(NULL != new_guild->name);

  new_guild->icon = calloc(1, MAX_HASH_LENGTH);
  assert(NULL != new_guild->icon);

  new_guild->discovery_splash = calloc(1, MAX_HASH_LENGTH);
  assert(NULL != new_guild->discovery_splash);

  new_guild->owner_id = calloc(1, SNOWFLAKE_INTERNAL_WORKER_ID);
  assert(NULL != new_guild->owner_id);

  new_guild->permissions_new = calloc(1, SNOWFLAKE_INCREMENT);
  assert(NULL != new_guild->permissions_new);

  new_guild->region = calloc(1, MAX_REGION_LENGTH);
  assert(NULL != new_guild->region);

  new_guild->afk_channel_id = calloc(1, SNOWFLAKE_INTERNAL_WORKER_ID);
  assert(NULL != new_guild->afk_channel_id);

  new_guild->embed_channel_id = calloc(1, SNOWFLAKE_INTERNAL_WORKER_ID);
  assert(NULL != new_guild->embed_channel_id);

  new_guild->application_id = calloc(1, SNOWFLAKE_INTERNAL_WORKER_ID);
  assert(NULL != new_guild->application_id);

  new_guild->widget_channel_id = calloc(1, SNOWFLAKE_INTERNAL_WORKER_ID);
  assert(NULL != new_guild->widget_channel_id);

  new_guild->system_channel_id = calloc(1, SNOWFLAKE_INTERNAL_WORKER_ID);
  assert(NULL != new_guild->system_channel_id);

  new_guild->rules_channel_id = calloc(1, SNOWFLAKE_INTERNAL_WORKER_ID);
  assert(NULL != new_guild->rules_channel_id);

  new_guild->joined_at = calloc(1, SNOWFLAKE_TIMESTAMP);
  assert(NULL != new_guild->joined_at);

  new_guild->vanity_url_code = calloc(1, SNOWFLAKE_INCREMENT);
  assert(NULL != new_guild->vanity_url_code);

  new_guild->description = calloc(1, DESCRIPTION_LENGTH);
  assert(NULL != new_guild->description);

  new_guild->banner = calloc(1, MAX_HASH_LENGTH);
  assert(NULL != new_guild->banner);

  new_guild->preferred_locale = calloc(1, MAX_LOCALE_LENGTH);
  assert(NULL != new_guild->preferred_locale);

  new_guild->public_updates_channel_id = calloc(1, SNOWFLAKE_INTERNAL_WORKER_ID);
  assert(NULL != new_guild->public_updates_channel_id);

  new_guild->easy_handle = curl_easy_custom_init(utils);
  assert(NULL != new_guild->easy_handle);

  return new_guild;
}

void
discord_guild_destroy(discord_guild_st *guild)
{
  free(guild->id);
  free(guild->name);
  free(guild->icon);
  free(guild->discovery_splash);
  free(guild->owner_id);
  free(guild->permissions_new);
  free(guild->region);
  free(guild->afk_channel_id);
  free(guild->embed_channel_id);
  free(guild->application_id);
  free(guild->widget_channel_id);
  free(guild->system_channel_id);
  free(guild->rules_channel_id);
  free(guild->joined_at);
  free(guild->vanity_url_code);
  free(guild->description);
  free(guild->banner);
  free(guild->preferred_locale);
  free(guild->public_updates_channel_id);

  curl_easy_cleanup(guild->easy_handle);

  free(guild);
}

void
discord_get_guild(discord_st *discord, char guild_id[])
{
  strcpy(discord->utils->url_route, "/guilds/");
  strcat(discord->utils->url_route, guild_id);

  // SET CURL_EASY DEFAULT CONFIG //
  discord_guild_st *guild = discord->guild;
  discord_request_get(guild->easy_handle, discord->utils);

  jsonc_scanf(
      discord->utils->response,
      "id%s,name%s,icon%s,owner%d,permissions%lld,permissions_new%s",
      guild->id,
      guild->name,
      guild->icon,
      (int*)&guild->owner,
      &guild->permissions,
      guild->permissions_new);
 
  /* UNCOMMENT FOR TESTING
  fprintf(stdout,
      "\njson: %s\nGUILD: %s %s %s %d %lld %s\n",
      buffer.response,
      guild->id,
      guild->name,
      guild->icon,
      guild->owner,
      guild->permissions,
      guild->permissions_new);
  */
}
