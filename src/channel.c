#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <assert.h>
#include <string.h>

#include "libconcord.h"

discord_channel_st*
discord_channel_init(discord_utils_st *utils)
{
  discord_channel_st *new_channel = discord_malloc(sizeof *new_channel);
  new_channel->id = discord_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);
  new_channel->guild_id = discord_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);
  new_channel->name = discord_malloc(NAME_LENGTH);
  new_channel->topic = discord_malloc(TOPIC_LENGTH);
  new_channel->last_message_id = discord_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);
  new_channel->icon = discord_malloc(MAX_HASH_LENGTH);
  new_channel->owner_id = discord_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);
  new_channel->application_id = discord_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);
  new_channel->parent_id = discord_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);
  new_channel->last_pin_timestamp = discord_malloc(SNOWFLAKE_TIMESTAMP);
  
  new_channel->hashtable = hashtable_init();
  hashtable_build(new_channel->hashtable, CLIST_HASHTABLE_SIZE);

  return new_channel;
}

void
discord_channel_destroy(discord_channel_st *channel)
{
  discord_free(channel->id);
  discord_free(channel->guild_id);
  discord_free(channel->name);
  discord_free(channel->topic);
  discord_free(channel->last_message_id);
  discord_free(channel->icon);
  discord_free(channel->owner_id);
  discord_free(channel->application_id);
  discord_free(channel->parent_id);
  discord_free(channel->last_pin_timestamp);

  if (NULL != channel->permission_overwrites){
    jscon_destroy(channel->permission_overwrites);
  }

  hashtable_destroy(channel->hashtable);
  discord_clist_free_all(channel->conn_list);

  discord_free(channel);
}

static void
_discord_ld_channel(void *ptr, struct curl_memory_s *chunk)
{
  discord_channel_st *channel = ptr;

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
      response,
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
  
  chunk->size = 0;
  discord_free(chunk->response);
}

void
discord_get_channel(discord_st* discord, char channel_id[])
{
  char url_route[256] = "/channels/";
  strcat(url_route, channel_id);

  discord_channel_st *channel = discord->channel;
  struct discord_clist_s *conn = discord_get_conn(
                                    discord,
                                    "GetChannel",
                                    channel->hashtable,
                                    &channel->conn_list,
                                    &_discord_ld_channel);

  discord_request_get(discord->utils, conn, url_route);

  if (SYNC == discord->utils->method){
    (*conn->load_cb)(channel, &conn->chunk);
  }
}
