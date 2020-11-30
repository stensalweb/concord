#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <string.h>

#include <libconcord.h>

#include "concord-common.h"
#include "debug.h"

concord_channel_t*
concord_channel_init()
{
  concord_channel_t *new_channel = safe_calloc(1, sizeof *new_channel);
  new_channel->id = safe_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);
  new_channel->guild_id = safe_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);
  new_channel->name = safe_malloc(MAX_NAME_LEN);
  new_channel->topic = safe_malloc(MAX_TOPIC_LEN);
  new_channel->last_message_id = safe_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);
  new_channel->icon = safe_malloc(MAX_HASH_LEN);
  new_channel->owner_id = safe_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);
  new_channel->application_id = safe_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);
  new_channel->parent_id = safe_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);
  new_channel->last_pin_timestamp = safe_malloc(SNOWFLAKE_TIMESTAMP);

  return new_channel;
}

void
concord_channel_destroy(concord_channel_t *channel)
{
  safe_free(channel->id);
  safe_free(channel->guild_id);
  safe_free(channel->name);
  safe_free(channel->topic);
  safe_free(channel->last_message_id);
  safe_free(channel->icon);
  safe_free(channel->owner_id);
  safe_free(channel->application_id);
  safe_free(channel->parent_id);
  safe_free(channel->last_pin_timestamp);

  if (NULL != channel->permission_overwrites){
    jscon_destroy(channel->permission_overwrites);
  }

  if (NULL != channel->messages){
    jscon_destroy(channel->messages);
  }

  safe_free(channel);
}

static void
_concord_load_channel(void **p_channel, struct concord_response_s *response_body)
{
  concord_channel_t *channel = *p_channel;

  jscon_scanf(response_body->str,
     "%d[position]" \
     "%b[nsfw]" \
     "%s[last_message_id]" \
     "%d[bitrate]" \
     "%s[owner_id]" \
     "%s[application_id]" \
     "%s[last_pin_timestamp]" \
     "%s[id]" \
     "%d[type]" \
     "%s[guild_id]" \
     "%ji[permission_overwrites]" \
     "%s[name]" \
     "%s[topic]" \
     "%d[user_limit]" \
     "%d[rate_limit_per_user]" \
     "%ji[recipients]" \
     "%s[icon]" \
     "%s[parent_id]",
      &channel->position,
      &channel->nsfw,
      channel->last_message_id,
      &channel->bitrate,
      channel->owner_id,
      channel->application_id,
      channel->last_pin_timestamp,
      channel->id,
      &channel->type,
      channel->guild_id,
      &channel->permission_overwrites,
      channel->name,
      channel->topic,
      &channel->user_limit,
      &channel->rate_limit_per_user,
      &channel->recipients,
      channel->icon,
      channel->parent_id);

  *p_channel = channel;
}

void
concord_get_channel(concord_t *concord, char channel_id[], concord_channel_t **p_channel)
{
  if (NULL == p_channel){
    p_channel = &concord->channel;
  }

  /* this is a template common to every function that deals with
      sending a request to the Discord API */
  Concord_api_request( 
    concord->api,
    (void**)p_channel,
    &_concord_load_channel,
    GET, CHANNELS, channel_id);
}

static void
_concord_load_channel_messages(void **p_channel, struct concord_response_s *response_body)
{
  concord_channel_t *channel = *p_channel;

  if (NULL != channel->messages){
    jscon_destroy(channel->messages);
  }

  channel->messages = jscon_parse(response_body->str);
  DEBUG_ASSERT(NULL != channel->messages, "Out of memory");

  *p_channel = channel;
}

void
concord_get_channel_messages(concord_t *concord, char channel_id[], concord_channel_t **p_channel)
{
  if (NULL == p_channel){
    p_channel = &concord->channel;
  }

  /* this is a template common to every function that deals with
      sending a request to the Discord API */
  Concord_api_request( 
    concord->api,
    (void**)p_channel,
    &_concord_load_channel_messages,
    GET, CHANNELS_MESSAGES, channel_id);
}
