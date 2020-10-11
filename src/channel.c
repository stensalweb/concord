#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <assert.h>
#include <string.h>

#include "libconcord.h"
#include "api_wrapper_private.h"

concord_channel_st*
concord_channel_init(concord_utils_st *utils)
{
  concord_channel_st *new_channel = concord_malloc(sizeof *new_channel);
  new_channel->id = concord_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);
  new_channel->guild_id = concord_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);
  new_channel->name = concord_malloc(NAME_LENGTH);
  new_channel->topic = concord_malloc(TOPIC_LENGTH);
  new_channel->last_message_id = concord_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);
  new_channel->icon = concord_malloc(MAX_HASH_LENGTH);
  new_channel->owner_id = concord_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);
  new_channel->application_id = concord_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);
  new_channel->parent_id = concord_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);
  new_channel->last_pin_timestamp = concord_malloc(SNOWFLAKE_TIMESTAMP);

  return new_channel;
}

void
concord_channel_destroy(concord_channel_st *channel)
{
  concord_free(channel->id);
  concord_free(channel->guild_id);
  concord_free(channel->name);
  concord_free(channel->topic);
  concord_free(channel->last_message_id);
  concord_free(channel->icon);
  concord_free(channel->owner_id);
  concord_free(channel->application_id);
  concord_free(channel->parent_id);
  concord_free(channel->last_pin_timestamp);

  if (NULL != channel->permission_overwrites){
    jscon_destroy(channel->permission_overwrites);
  }

  concord_free(channel);
}

static void
_concord_ld_channel(void **p_channel, struct curl_memory_s *chunk)
{
  concord_channel_st *channel = *p_channel;

  jscon_scanf(chunk->response,
     "#position%jd \
      #nsfw%jb \
      #last_message_id%js \
      #bitrate%jd \
      #owner_id%js \
      #application_id%js \
      #last_pin_timestamp%js \
      #id%js \
      #type%jd \
      #guild_id%js \
      #permission_overwrites%ji \
      #name%js \
      #topic%js \
      #user_limit%jd \
      #rate_limit_per_user%jd \
      #recipients%ji \
      #icon%js \
      #parent_id%js",
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

  /*//UNCOMMENT FOR TESTING
  fprintf(stdout,
      "\njson: %s\nCHANNEL: %lld %d %s %lld %s %s %s %s %lld %s %p %s %s %lld %lld %p %s %s\n",
      chunk->response,
      channel->position,
      channel->nsfw,
      channel->last_message_id,
      channel->bitrate,
      channel->owner_id,
      channel->application_id,
      channel->last_pin_timestamp,
      channel->id,
      channel->type,
      channel->guild_id,
      (void*)channel->permission_overwrites,
      channel->name,
      channel->topic,
      channel->user_limit,
      channel->rate_limit_per_user,
      (void*)channel->recipients,
      channel->icon,
      channel->parent_id);
  */

  *p_channel = channel;
  
  chunk->size = 0;
  concord_free(chunk->response);
}

void
concord_get_channel(concord_st* concord, char channel_id[], concord_channel_st **p_channel)
{
  char endpoint[ENDPOINT_LENGTH] = "/channels/";
  strcat(endpoint, channel_id);

  if (NULL == p_channel){
    *p_channel = concord_channel_init(concord->utils);
  }

  /* this is a template common to every function that deals with
      sending a request to the Discord API */
  Concord_request_perform( 
    concord->utils,
    (void**)p_channel,
    endpoint,
    &_concord_ld_channel,
    &Concord_GET);
}
