#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <assert.h>
#include <string.h>

#include "libconcord.h"

discord_guild_st*
discord_guild_init(discord_utils_st* utils)
{
  discord_guild_st *new_guild = discord_malloc(sizeof *new_guild);
  new_guild->id = discord_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);
  new_guild->name = discord_malloc(NAME_LENGTH);
  new_guild->icon = discord_malloc(MAX_HASH_LENGTH);
  new_guild->discovery_splash = discord_malloc(MAX_HASH_LENGTH);
  new_guild->owner_id = discord_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);
  new_guild->permissions_new = discord_malloc(SNOWFLAKE_INCREMENT);
  new_guild->region = discord_malloc(MAX_REGION_LENGTH);
  new_guild->afk_channel_id = discord_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);
  new_guild->embed_channel_id = discord_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);
  new_guild->application_id = discord_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);
  new_guild->widget_channel_id = discord_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);
  new_guild->system_channel_id = discord_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);
  new_guild->rules_channel_id = discord_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);
  new_guild->joined_at = discord_malloc(SNOWFLAKE_TIMESTAMP);
  new_guild->vanity_url_code = discord_malloc(SNOWFLAKE_INCREMENT);
  new_guild->description = discord_malloc(DESCRIPTION_LENGTH);
  new_guild->banner = discord_malloc(MAX_HASH_LENGTH);
  new_guild->preferred_locale = discord_malloc(MAX_LOCALE_LENGTH);
  new_guild->public_updates_channel_id = discord_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);

  new_guild->conn_list = discord_clist_append(utils, new_guild->conn_list);

  return new_guild;
}

void
discord_guild_destroy(discord_guild_st *guild)
{
  discord_free(guild->id);
  discord_free(guild->name);
  discord_free(guild->icon);
  discord_free(guild->discovery_splash);
  discord_free(guild->owner_id);
  discord_free(guild->permissions_new);
  discord_free(guild->region);
  discord_free(guild->afk_channel_id);
  discord_free(guild->embed_channel_id);
  discord_free(guild->application_id);
  discord_free(guild->widget_channel_id);
  discord_free(guild->system_channel_id);
  discord_free(guild->rules_channel_id);
  discord_free(guild->joined_at);
  discord_free(guild->vanity_url_code);
  discord_free(guild->description);
  discord_free(guild->banner);
  discord_free(guild->preferred_locale);
  discord_free(guild->public_updates_channel_id);

  if (NULL != guild->channels){
    jscon_destroy(guild->channels);
  }

  discord_clist_free_all(guild->conn_list);

  discord_free(guild);
}

static void
_discord_get_guild_apply(discord_guild_st *guild, struct curl_memory_s *chunk)
{
  jscon_scanf(chunk->response,
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

  chunk->size = 0;
  discord_free(chunk->response);
}

void
discord_get_guild(discord_st *discord, char guild_id[])
{
  char url_route[256] = "/guilds/";
  strcat(url_route, guild_id);

  // SET CURL_EASY DEFAULT CONFIG //
  discord_guild_st *guild = discord->guild;
  struct discord_clist_s *conn_list = guild->conn_list;
  discord_request_get(discord->utils, conn_list, url_route);

  if (ASYNC == discord->utils->method) return;

  _discord_get_guild_apply(guild, &conn_list->chunk);
}

static void
_discord_get_guild_channels_apply(discord_guild_st *guild, struct curl_memory_s *chunk)
{
  if (NULL != guild->channels){
    jscon_destroy(guild->channels);
  }

  guild->channels = jscon_parse(chunk->response);

  chunk->size = 0;
  discord_free(chunk->response);
}

void
discord_get_guild_channels(discord_st *discord, char guild_id[])
{
  char url_route[256];
  sprintf(url_route, "/guilds/%s/channels", guild_id);

  // SET CURL_EASY DEFAULT CONFIG //
  discord_guild_st *guild = discord->guild;
  struct discord_clist_s *conn_list = guild->conn_list;
  discord_request_get(discord->utils, conn_list, url_route);

  if (ASYNC == discord->utils->method) return;

  _discord_get_guild_channels_apply(guild, &conn_list->chunk);
}
