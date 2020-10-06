#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <assert.h>
#include <string.h>

#include "httpclient.h"
#include "libconcord.h"

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
  if (NULL != guild->id)
    free(guild->id);
  if (NULL != guild->name)
    free(guild->name);
  if (NULL != guild->icon)
    free(guild->icon);
  if (NULL != guild->discovery_splash)
    free(guild->discovery_splash);
  if (NULL != guild->owner_id)
    free(guild->owner_id);
  if (NULL != guild->permissions_new)
    free(guild->permissions_new);
  if (NULL != guild->region)
    free(guild->region);
  if (NULL != guild->afk_channel_id)
    free(guild->afk_channel_id);
  if (NULL != guild->embed_channel_id)
    free(guild->embed_channel_id);
  if (NULL != guild->application_id)
    free(guild->application_id);
  if (NULL != guild->widget_channel_id)
    free(guild->widget_channel_id);
  if (NULL != guild->system_channel_id)
    free(guild->system_channel_id);
  if (NULL != guild->rules_channel_id)
    free(guild->rules_channel_id);
  if (NULL != guild->joined_at)
    free(guild->joined_at);
  if (NULL != guild->vanity_url_code)
    free(guild->vanity_url_code);
  if (NULL != guild->description)
    free(guild->description);
  if (NULL != guild->banner)
    free(guild->banner);
  if (NULL != guild->preferred_locale)
    free(guild->preferred_locale);
  if (NULL != guild->public_updates_channel_id)
    free(guild->public_updates_channel_id);

  if (NULL != guild->channels)
    jscon_destroy(guild->channels);

  curl_easy_cleanup(guild->easy_handle);

  free(guild);
}

void
discord_get_guild(discord_st *discord, char guild_id[])
{
  char url_route[256] = "/guilds/";
  strcat(url_route, guild_id);

  // SET CURL_EASY DEFAULT CONFIG //
  discord_guild_st *guild = discord->guild;
  char *response = discord_request_get(guild->easy_handle, url_route);

  jscon_scanf(response,
      "#id%js \
      #name%js \
      #icon%js \
      #owner%jb \
      #permissions%jd \
      #permissions_new%js",
      guild->id,
      guild->name,
      guild->icon,
      &guild->owner,
      &guild->permissions,
      guild->permissions_new);
 
  /* UNCOMMENT FOR TESTING
  fprintf(stdout,
      "\njson: %s\nGUILD: %s %s %s %d %lld %s\n",
      response,
      guild->id,
      guild->name,
      guild->icon,
      guild->owner,
      guild->permissions,
      guild->permissions_new);
  */

  free(response);
  response = NULL;
}

void
discord_get_guild_channels(discord_st *discord, char guild_id[])
{
  char url_route[256];
  sprintf(url_route, "/guilds/%s/channels", guild_id);

  // SET CURL_EASY DEFAULT CONFIG //
  discord_guild_st *guild = discord->guild;
  char *response = discord_request_get(guild->easy_handle, url_route);

  fprintf(stdout, "%s\n", response);

  if (NULL != guild->channels){
    jscon_destroy(guild->channels);
  }

  guild->channels = jscon_parse(response);
  assert(NULL != guild->channels);

  free(response);
  response = NULL;
}
