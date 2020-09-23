#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <assert.h>
#include <string.h>

#include "global_config.h"
#include "REST.h"
#include "libdiscordc.h"

discord_channel_st*
discord_channel_init()
{
  discord_channel_st *new_channel = calloc(1, sizeof *new_channel);
  assert(NULL != new_channel);

  new_channel->easy_handle = curl_easy_init();
  assert(NULL != new_channel->easy_handle);

  return new_channel;
}

void
discord_channel_destroy(discord_channel_st *channel)
{
  if (NULL != channel->permission_overwrites)
    jsonc_destroy(channel->permission_overwrites);

  curl_easy_cleanup(channel->easy_handle);

  free(channel);
}

void
discord_get_channel(discord_channel_st* channel, char channel_id[])
{
  strcpy(g_config.url_route, "/channels/");
  strcat(g_config.url_route, channel_id);

  // SET CURL_EASY DEFAULT CONFIG //
  api_response_st buffer = {0};
  curl_easy_set_write(channel->easy_handle, &buffer);

  jsonc_sscanf(
      buffer.response,
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

  /* UNCOMMENT FOR TESTING
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
