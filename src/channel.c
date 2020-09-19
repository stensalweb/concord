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
  if (NULL != channel->last_message_id)
    free(channel->last_message_id);
  if (NULL != channel->owner_id)
    free(channel->owner_id);
  if (NULL != channel->application_id)
    free(channel->application_id);
  if (NULL != channel->last_pin_timestamp)
    free(channel->last_pin_timestamp);
  if (NULL != channel->id)
    free(channel->id);
  if (NULL != channel->guild_id)
    free(channel->guild_id);
  if (NULL != channel->permission_overwrites)
    jsonc_destroy(channel->permission_overwrites);
  if (NULL != channel->name)
    free(channel->name);
  if (NULL != channel->topic)
    free(channel->topic);
  if (NULL != channel->recipients)
    jsonc_destroy(channel->recipients);
  if (NULL != channel->icon)
    free(channel->icon);
  if (NULL != channel->parent_id)
    free(channel->parent_id);

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

  jsonc_item_st *root = jsonc_parse(buffer.response);
  assert(NULL != root);

  jsonc_item_st *tmp;

  tmp = jsonc_get_branch(root,"position");
  channel->position = jsonc_get_integer(tmp);

  tmp = jsonc_get_branch(root,"nsfw");
  channel->nsfw = jsonc_get_boolean(tmp);

  tmp = jsonc_get_branch(root,"last_message_id");
  channel->last_message_id = jsonc_strdup(tmp);

  tmp = jsonc_get_branch(root,"bitrate");
  channel->bitrate = jsonc_get_integer(tmp);

  tmp = jsonc_get_branch(root,"owner_id");
  channel->owner_id = jsonc_strdup(tmp);

  tmp = jsonc_get_branch(root,"application_id");
  channel->application_id = jsonc_strdup(tmp);

  tmp = jsonc_get_branch(root,"last_pin_timestamp");
  channel->last_pin_timestamp = jsonc_strdup(tmp);

  tmp = jsonc_get_branch(root,"id");
  channel->id = jsonc_strdup(tmp);

  tmp = jsonc_get_branch(root,"type");
  channel->type = jsonc_get_integer(tmp);

  tmp = jsonc_get_branch(root,"guild_id");
  channel->guild_id = jsonc_strdup(tmp);

  tmp = jsonc_get_branch(root,"permission_overwrites");
  channel->permission_overwrites = jsonc_clone(tmp);

  tmp = jsonc_get_branch(root,"name");
  channel->name = jsonc_strdup(tmp);

  tmp = jsonc_get_branch(root,"topic");
  channel->topic = jsonc_strdup(tmp);

  tmp = jsonc_get_branch(root,"user_limit");
  channel->user_limit = jsonc_get_integer(tmp);

  tmp = jsonc_get_branch(root,"rate_limit_per_user");
  channel->rate_limit_per_user = jsonc_get_integer(tmp);

  tmp = jsonc_get_branch(root,"recipients");
  channel->recipients = jsonc_clone(tmp);

  tmp = jsonc_get_branch(root,"icon");
  channel->icon = jsonc_strdup(tmp);

  tmp = jsonc_get_branch(root,"parent_id");
  channel->parent_id = jsonc_strdup(tmp);

  jsonc_destroy(root);
}
