#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <string.h>

#include <libconcord.h>

#include "debug.h"
#include "concord-common.h"

concord_guild_st*
concord_guild_init()
{
  concord_guild_st *new_guild = safe_malloc(sizeof *new_guild);
  new_guild->id = safe_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);
  new_guild->name = safe_malloc(MAX_NAME_LEN);
  new_guild->icon = safe_malloc(MAX_HASH_LEN);
  new_guild->discovery_splash = safe_malloc(MAX_HASH_LEN);
  new_guild->owner_id = safe_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);
  new_guild->permissions_new = safe_malloc(SNOWFLAKE_INCREMENT);
  new_guild->region = safe_malloc(MAX_REGION_LEN);
  new_guild->afk_channel_id = safe_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);
  new_guild->embed_channel_id = safe_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);
  new_guild->application_id = safe_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);
  new_guild->widget_channel_id = safe_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);
  new_guild->system_channel_id = safe_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);
  new_guild->rules_channel_id = safe_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);
  new_guild->joined_at = safe_malloc(SNOWFLAKE_TIMESTAMP);
  new_guild->vanity_url_code = safe_malloc(SNOWFLAKE_INCREMENT);
  new_guild->description = safe_malloc(MAX_DESCRIPTION_LEN);
  new_guild->banner = safe_malloc(MAX_HASH_LEN);
  new_guild->preferred_locale = safe_malloc(MAX_LOCALE_LEN);
  new_guild->public_updates_channel_id = safe_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);

  return new_guild;
}

void
concord_guild_destroy(concord_guild_st *guild)
{
  safe_free(guild->id);
  safe_free(guild->name);
  safe_free(guild->icon);
  safe_free(guild->discovery_splash);
  safe_free(guild->owner_id);
  safe_free(guild->permissions_new);
  safe_free(guild->region);
  safe_free(guild->afk_channel_id);
  safe_free(guild->embed_channel_id);
  safe_free(guild->application_id);
  safe_free(guild->widget_channel_id);
  safe_free(guild->system_channel_id);
  safe_free(guild->rules_channel_id);
  safe_free(guild->joined_at);
  safe_free(guild->vanity_url_code);
  safe_free(guild->description);
  safe_free(guild->banner);
  safe_free(guild->preferred_locale);
  safe_free(guild->public_updates_channel_id);

  if (NULL != guild->channels){
    jscon_destroy(guild->channels);
  }

  safe_free(guild);
}

static void
_concord_load_guild(void **p_guild, struct concord_response_s *response_body)
{
  concord_guild_st *guild = *p_guild;

  jscon_scanf(response_body->str,
     "#id%js " \
     "#name%js " \
     "#icon%js " \
     "#owner%jb " \
     "#permissions%jd " \
     "#permissions_new%js",
      guild->id,
      guild->name,
      guild->icon,
      &guild->owner,
      &guild->permissions,
      guild->permissions_new);
 
  /* UNCOMMENT FOR TESTING
  fprintf(stdout,
      "\njson: %s\n\
      \nGUILD:\n\
      \"id\": %s\n\
      \"name\": %s\n\
      \"icon\": %s\n\
      \"owner\": %d\n\
      \"permissions\": %lld\n\
      \"permissions_new\": %s\n",
      response_body->str,
      guild->id,
      guild->name,
      guild->icon,
      guild->owner,
      guild->permissions,
      guild->permissions_new);
  */

  *p_guild = guild;
}

void
concord_get_guild(concord_st *concord, char guild_id[], concord_guild_st **p_guild)
{
  if (NULL == p_guild){
    p_guild = &concord->guild;
  }

  /* this is a template common to every function that deals with
      sending a request to the Discord API */
  Concord_http_request( 
    concord->http,
    (void**)p_guild,
    &_concord_load_guild,
    GET, GUILDS, guild_id);
}

static void
_concord_load_guild_channels(void **p_guild, struct concord_response_s *response_body)
{
  concord_guild_st *guild = *p_guild;

  if (NULL != guild->channels){
    jscon_destroy(guild->channels);
  }

  guild->channels = jscon_parse(response_body->str);
  DEBUG_ASSERT(NULL != guild->channels, "Out of memory");

  *p_guild = guild;
}

void
concord_get_guild_channels(concord_st *concord, char guild_id[], concord_guild_st **p_guild)
{
  if (NULL == p_guild){
    p_guild = &concord->guild;
  }

  /* this is a template common to every function that deals with
      sending a request to the Discord API */
  Concord_http_request( 
    concord->http,
    (void**)p_guild,
    &_concord_load_guild_channels,
    GET, GUILDS_CHANNELS, guild_id);
}
