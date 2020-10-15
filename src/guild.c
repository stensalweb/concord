#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <assert.h>
#include <string.h>

//#include <curl/curl.h>
//#include <libjscon.h>

#include <libconcord.h>

#include "api_wrapper_private.h"

concord_guild_st*
concord_guild_init(concord_utils_st* utils)
{
  concord_guild_st *new_guild = concord_malloc(sizeof *new_guild);
  new_guild->id = concord_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);
  new_guild->name = concord_malloc(NAME_LENGTH);
  new_guild->icon = concord_malloc(MAX_HASH_LENGTH);
  new_guild->discovery_splash = concord_malloc(MAX_HASH_LENGTH);
  new_guild->owner_id = concord_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);
  new_guild->permissions_new = concord_malloc(SNOWFLAKE_INCREMENT);
  new_guild->region = concord_malloc(MAX_REGION_LENGTH);
  new_guild->afk_channel_id = concord_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);
  new_guild->embed_channel_id = concord_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);
  new_guild->application_id = concord_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);
  new_guild->widget_channel_id = concord_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);
  new_guild->system_channel_id = concord_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);
  new_guild->rules_channel_id = concord_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);
  new_guild->joined_at = concord_malloc(SNOWFLAKE_TIMESTAMP);
  new_guild->vanity_url_code = concord_malloc(SNOWFLAKE_INCREMENT);
  new_guild->description = concord_malloc(DESCRIPTION_LENGTH);
  new_guild->banner = concord_malloc(MAX_HASH_LENGTH);
  new_guild->preferred_locale = concord_malloc(MAX_LOCALE_LENGTH);
  new_guild->public_updates_channel_id = concord_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);

  return new_guild;
}

void
concord_guild_destroy(concord_guild_st *guild)
{
  concord_free(guild->id);
  concord_free(guild->name);
  concord_free(guild->icon);
  concord_free(guild->discovery_splash);
  concord_free(guild->owner_id);
  concord_free(guild->permissions_new);
  concord_free(guild->region);
  concord_free(guild->afk_channel_id);
  concord_free(guild->embed_channel_id);
  concord_free(guild->application_id);
  concord_free(guild->widget_channel_id);
  concord_free(guild->system_channel_id);
  concord_free(guild->rules_channel_id);
  concord_free(guild->joined_at);
  concord_free(guild->vanity_url_code);
  concord_free(guild->description);
  concord_free(guild->banner);
  concord_free(guild->preferred_locale);
  concord_free(guild->public_updates_channel_id);

  if (NULL != guild->channels){
    jscon_destroy(guild->channels);
  }

  concord_free(guild);
}

static void
_concord_ld_guild(void **p_guild, struct curl_response_s *response_body)
{
  concord_guild_st *guild = *p_guild;

  jscon_scanf(response_body->str,
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
  char endpoint[ENDPOINT_LENGTH] = "/guilds/";
  strcat(endpoint, guild_id);

  if (NULL == p_guild){
    p_guild = &concord->guild;
  }

  /* this is a template common to every function that deals with
      sending a request to the Discord API */
  Concord_perform_request( 
    concord->utils,
    (void**)p_guild,
    "GetGuild",
    endpoint,
    &_concord_ld_guild,
    &Concord_GET);
}

static void
_concord_ld_guild_channels(void **p_guild, struct curl_response_s *response_body)
{
  concord_guild_st *guild = *p_guild;

  if (NULL != guild->channels){
    jscon_destroy(guild->channels);
  }

  guild->channels = jscon_parse(response_body->str);

  *p_guild = guild;
}

void
concord_get_guild_channels(concord_st *concord, char guild_id[], concord_guild_st **p_guild)
{
  char endpoint[ENDPOINT_LENGTH];
  sprintf(endpoint, "/guilds/%s/channels", guild_id);

  if (NULL == p_guild){
    p_guild = &concord->guild;
  }

  /* this is a template common to every function that deals with
      sending a request to the Discord API */
  Concord_perform_request( 
    concord->utils,
    (void**)p_guild,
    "GetGuildChannels",
    endpoint,
    &_concord_ld_guild_channels,
    &Concord_GET);
}
