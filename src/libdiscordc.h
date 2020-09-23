#ifndef LIBDISCC_H_
#define LIBDISCC_H_

#include <curl/curl.h>

#include "../JSONc/include/libjsonc.h"

/* SNOWFLAKES
https://discord.com/developers/docs/reference#snowflakes */
typedef enum {
  SNOWFLAKE_INCREMENT           = 12,
  SNOWFLAKE_PROCESS_ID          = 17,
  SNOWFLAKE_INTERNAL_WORKER_ID  = 22,
  SNOWFLAKE_TIMESTAMP           = 64,
} discordc_snowflake_et;

typedef enum {
  NAME_LENGTH           = 100,
  TOPIC_LENGTH          = 1024,
  DESCRIPTION_LENGTH    = 1024,
  USERNAME_LENGTH       = 32,
  DISCRIMINATOR_LENGTH  = 4,
  MAX_HASH_LENGTH       = 1024,
  MAX_LOCALE_LENGTH     = 15,
  MAX_EMAIL_LENGTH      = 254,
} discordc_limits_et;

/* CHANNEL TYPES
https://discord.com/developers/docs/resources/channel#channel-object-channel-types */
typedef enum {
  GUILD_TEXT            = 0,
  DM                    = 1,
  GUILD_VOICE           = 2,
  GROUP_DM              = 3,
  GUILD_CATEGORY        = 4,
  GUILD_NEWS            = 5,
  GUILD_STORE           = 6,
} discordc_channel_types_et;

/* CHANNEL OBJECT
https://discord.com/developers/docs/resources/channel#channel-object-channel-structure */
typedef struct {
  jsonc_char_kt id[SNOWFLAKE_INTERNAL_WORKER_ID];
  jsonc_integer_kt type;
  jsonc_char_kt guild_id[SNOWFLAKE_INTERNAL_WORKER_ID];
  jsonc_integer_kt position;
  jsonc_item_st *permission_overwrites;
  jsonc_char_kt name[NAME_LENGTH];
  jsonc_char_kt topic[TOPIC_LENGTH];
  jsonc_boolean_kt nsfw;
  jsonc_char_kt last_message_id[SNOWFLAKE_INTERNAL_WORKER_ID];
  jsonc_integer_kt bitrate;
  jsonc_integer_kt user_limit;
  jsonc_integer_kt rate_limit_per_user;
  jsonc_item_st *recipients;
  jsonc_char_kt icon[MAX_HASH_LENGTH];
  jsonc_char_kt owner_id[SNOWFLAKE_INTERNAL_WORKER_ID];
  jsonc_char_kt application_id[SNOWFLAKE_INTERNAL_WORKER_ID];
  jsonc_char_kt parent_id[SNOWFLAKE_INTERNAL_WORKER_ID];
  jsonc_char_kt last_pin_timestamp[SNOWFLAKE_TIMESTAMP];

  CURL *easy_handle;
} discord_channel_st;

discord_channel_st* discord_channel_init();
void discord_channel_destroy(discord_channel_st *channel);
void discord_get_channel(discord_channel_st *channel, char channel_id[]);

/* GUILD OBJECT
https://discord.com/developers/docs/resources/guild#guild-object-guild-structure */
typedef struct {
  jsonc_char_kt id[SNOWFLAKE_INTERNAL_WORKER_ID];
  jsonc_char_kt name[NAME_LENGTH];
  jsonc_char_kt icon[MAX_HASH_LENGTH];
  jsonc_char_kt splash[MAX_HASH_LENGTH];
  jsonc_char_kt discovery_splash[MAX_HASH_LENGTH];
  jsonc_boolean_kt owner;
  jsonc_char_kt owner_id[SNOWFLAKE_INTERNAL_WORKER_ID];
  jsonc_integer_kt permissions;
  jsonc_char_kt permissions_new[SNOWFLAKE_INCREMENT];
  jsonc_char_kt *region;
  jsonc_char_kt afk_channel_id[SNOWFLAKE_INTERNAL_WORKER_ID];
  jsonc_integer_kt afk_timeout;
  jsonc_boolean_kt embed_enabled;
  jsonc_char_kt embed_channel_id[SNOWFLAKE_INTERNAL_WORKER_ID];
  jsonc_integer_kt verification_level;
  jsonc_integer_kt default_message_notifications;
  jsonc_integer_kt explicit_content_filter;
  jsonc_item_st *roles;
  jsonc_item_st *emojis;
  jsonc_item_st *features;
  jsonc_integer_kt mfa_level;
  jsonc_char_kt application_id[SNOWFLAKE_INTERNAL_WORKER_ID];
  jsonc_boolean_kt widget_enabled;
  jsonc_char_kt widget_channel_id[SNOWFLAKE_INTERNAL_WORKER_ID];
  jsonc_char_kt system_channel_id[SNOWFLAKE_INTERNAL_WORKER_ID];
  jsonc_integer_kt system_channel_flags;
  jsonc_char_kt rules_channel_id[SNOWFLAKE_INTERNAL_WORKER_ID];
  jsonc_char_kt joined_at[SNOWFLAKE_TIMESTAMP];
  jsonc_boolean_kt large;
  jsonc_boolean_kt unavailable;
  jsonc_integer_kt member_count;
  jsonc_item_st *voice_states;
  jsonc_item_st *members;
  jsonc_item_st *channels;
  jsonc_item_st *presences;
  jsonc_integer_kt max_presences;
  jsonc_integer_kt mas_members;
  jsonc_char_kt vanity_url_code[SNOWFLAKE_INCREMENT];
  jsonc_char_kt description[DESCRIPTION_LENGTH];
  jsonc_char_kt banner[MAX_HASH_LENGTH];
  jsonc_integer_kt premium_tier;
  jsonc_integer_kt premium_subscription_count;
  jsonc_char_kt preferred_locale[MAX_LOCALE_LENGTH];
  jsonc_char_kt public_updates_channel_id[SNOWFLAKE_INTERNAL_WORKER_ID];
  jsonc_integer_kt max_video_channel_users;
  jsonc_integer_kt approximate_member_count;
  jsonc_integer_kt approximate_presence_count;

  CURL *easy_handle;
} discord_guild_st;


discord_guild_st* discord_guild_init();
void discord_guild_destroy(discord_guild_st *guild);
void discord_get_guild(discord_guild_st *guild, char guild_id[]);

/* USER OBJECT
https://discord.com/developers/docs/resources/user#user-object-user-structure */
typedef struct {
  jsonc_char_kt id[SNOWFLAKE_INTERNAL_WORKER_ID];
  jsonc_char_kt username[USERNAME_LENGTH];
  jsonc_char_kt discriminator[DISCRIMINATOR_LENGTH];
  jsonc_char_kt avatar[MAX_HASH_LENGTH];
  jsonc_boolean_kt bot;
  jsonc_boolean_kt sys;
  jsonc_boolean_kt mfa_enabled;
  jsonc_char_kt locale[MAX_LOCALE_LENGTH];
  jsonc_boolean_kt verified;
  jsonc_char_kt email[MAX_EMAIL_LENGTH];
  jsonc_integer_kt flags;
  jsonc_integer_kt premium_type;
  jsonc_integer_kt public_flags;

  jsonc_item_st *guilds;

  CURL *easy_handle;
} discord_user_st;


discord_user_st* discord_user_init();
void discord_user_destroy(discord_user_st *user);
void discord_get_client(discord_user_st *user);
void discord_get_user(discord_user_st *user, char user_id[]);
void discord_get_client_guilds(discord_user_st* client);

typedef struct {
  discord_channel_st *channel;
  discord_user_st *user;
  discord_user_st *client;
  discord_guild_st *guild;
} discord_st;

discord_st* discord_init(char *bot_token);
void discord_cleanup(discord_st* discord);

#endif
