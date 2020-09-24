#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <assert.h>
#include <string.h>

#include "REST.h"
#include "libdiscordc.h"

discord_channel_st*
discord_channel_init(discord_utils_st *utils)
{
  discord_channel_st *new_channel = calloc(1, sizeof *new_channel);
  assert(NULL != new_channel);

  new_channel->id = calloc(1, SNOWFLAKE_INTERNAL_WORKER_ID);
  assert(NULL != new_channel->id);

  new_channel->guild_id = calloc(1, SNOWFLAKE_INTERNAL_WORKER_ID);
  assert(NULL != new_channel->guild_id);

  new_channel->name = calloc(1, NAME_LENGTH);
  assert(NULL != new_channel->name);
  
  new_channel->topic = calloc(1, TOPIC_LENGTH);
  assert(NULL != new_channel->topic);
  
  new_channel->last_message_id = calloc(1, SNOWFLAKE_INTERNAL_WORKER_ID);
  assert(NULL != new_channel->last_message_id);
  
  new_channel->icon = calloc(1, MAX_HASH_LENGTH);
  assert(NULL != new_channel->icon);
  
  new_channel->owner_id = calloc(1, SNOWFLAKE_INTERNAL_WORKER_ID);
  assert(NULL != new_channel->owner_id);
  
  new_channel->application_id = calloc(1, SNOWFLAKE_INTERNAL_WORKER_ID);
  assert(NULL != new_channel->application_id);
  
  new_channel->parent_id = calloc(1, SNOWFLAKE_INTERNAL_WORKER_ID);
  assert(NULL != new_channel->parent_id);
  
  new_channel->last_pin_timestamp = calloc(1, SNOWFLAKE_TIMESTAMP);
  assert(NULL != new_channel->last_pin_timestamp);
  
  new_channel->easy_handle = curl_easy_custom_init(utils);
  assert(NULL != new_channel->easy_handle);

  return new_channel;
}

void
discord_channel_destroy(discord_channel_st *channel)
{
  if (NULL != channel->permission_overwrites)
    jsonc_destroy(channel->permission_overwrites);

  free(channel->id);
  free(channel->guild_id);
  free(channel->name);
  free(channel->topic);
  free(channel->last_message_id);
  free(channel->icon);
  free(channel->owner_id);
  free(channel->application_id);
  free(channel->parent_id);
  free(channel->last_pin_timestamp);

  curl_easy_cleanup(channel->easy_handle);

  free(channel);
}

void
discord_get_channel(discord_st* discord, char channel_id[])
{
  strcpy(discord->utils->url_route, "/channels/");
  strcat(discord->utils->url_route, channel_id);

  // SET CURL_EASY DEFAULT CONFIG //
  discord_channel_st *channel = discord->channel;
  discord_request_get(channel->easy_handle, discord->utils);

  jsonc_sscanf(
      discord->utils->response,
      "position%lld,nsfw%d,last_message_id%s,bitrate%lld,owner_id%s,application_id%s,last_pin_timestamp%s,id%s,type%lld,guild_id%s,permission_overwrites%p,name%s,topic%s,user_limit%lld,rate_limit_per_user%lld,recipients%p,icon%s,parent_id%s",
      &channel->position,
      (int*)&channel->nsfw,
      channel->last_message_id,
      &channel->bitrate,
      channel->owner_id,
      channel->application_id,
      channel->last_pin_timestamp,
      channel->id,
      &channel->type,
      channel->guild_id,
      (void**)&channel->permission_overwrites,
      channel->name,
      channel->topic,
      &channel->user_limit,
      &channel->rate_limit_per_user,
      (void**)&channel->recipients,
      channel->icon,
      channel->parent_id);

  /*//UNCOMMENT FOR TESTING
  fprintf(stdout,
      "\njson: %s\nCHANNEL: %lld %d %s %lld %s %s %s %s %lld %s %p %s %s %lld %lld %p %s %s\n",
      buffer.response,
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
}
