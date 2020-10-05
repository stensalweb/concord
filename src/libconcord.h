#ifndef LIBDISCC_H_
#define LIBDISCC_H_

#include <curl/curl.h>

#include "../JSCON/include/libjscon.h"

#define MAX_RESPONSE_LENGTH 1 << 15
#define MAX_HEADER_LENGTH 1 << 9

/* SNOWFLAKES
https://discord.com/developers/docs/reference#snowflakes */
typedef enum {
  SNOWFLAKE_INCREMENT           = 12,
  SNOWFLAKE_PROCESS_ID          = 17,
  SNOWFLAKE_INTERNAL_WORKER_ID  = 22,
  SNOWFLAKE_TIMESTAMP           = 64,
} concord_snowflake_et;

typedef enum {
  NAME_LENGTH           = 100,
  TOPIC_LENGTH          = 1024,
  DESCRIPTION_LENGTH    = 1024,
  USERNAME_LENGTH       = 32,
  DISCRIMINATOR_LENGTH  = 4,
  MAX_HASH_LENGTH       = 1024,
  MAX_LOCALE_LENGTH     = 15,
  MAX_EMAIL_LENGTH      = 254,
  MAX_REGION_LENGTH     = 15,
} concord_limits_et;

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
} concord_channel_types_et;

/* CHANNEL OBJECT
https://discord.com/developers/docs/resources/channel#channel-object-channel-structure */
typedef struct {
  jscon_char_kt *id;
  jscon_integer_kt type;
  jscon_char_kt *guild_id;
  jscon_integer_kt position;
  jscon_item_st *permission_overwrites;
  jscon_char_kt *name;
  jscon_char_kt *topic;
  jscon_boolean_kt nsfw;
  jscon_char_kt *last_message_id;
  jscon_integer_kt bitrate;
  jscon_integer_kt user_limit;
  jscon_integer_kt rate_limit_per_user;
  jscon_item_st *recipients;
  jscon_char_kt *icon;
  jscon_char_kt *owner_id;
  jscon_char_kt *application_id;
  jscon_char_kt *parent_id;
  jscon_char_kt *last_pin_timestamp;

  CURL *easy_handle;
} discord_channel_st;

/* GUILD OBJECT
https://discord.com/developers/docs/resources/guild#guild-object-guild-structure */
typedef struct {
  jscon_char_kt *id;
  jscon_char_kt *name;
  jscon_char_kt *icon;
  jscon_char_kt *splash;
  jscon_char_kt *discovery_splash;
  jscon_boolean_kt owner;
  jscon_char_kt *owner_id;
  jscon_integer_kt permissions;
  jscon_char_kt *permissions_new;
  jscon_char_kt *region;
  jscon_char_kt *afk_channel_id;
  jscon_integer_kt afk_timeout;
  jscon_boolean_kt embed_enabled;
  jscon_char_kt *embed_channel_id;
  jscon_integer_kt verification_level;
  jscon_integer_kt default_message_notifications;
  jscon_integer_kt explicit_content_filter;
  jscon_item_st *roles;
  jscon_item_st *emojis;
  jscon_item_st *features;
  jscon_integer_kt mfa_level;
  jscon_char_kt *application_id;
  jscon_boolean_kt widget_enabled;
  jscon_char_kt *widget_channel_id;
  jscon_char_kt *system_channel_id;
  jscon_integer_kt system_channel_flags;
  jscon_char_kt *rules_channel_id;
  jscon_char_kt *joined_at;
  jscon_boolean_kt large;
  jscon_boolean_kt unavailable;
  jscon_integer_kt member_count;
  jscon_item_st *voice_states;
  jscon_item_st *members;
  jscon_item_st *channels;
  jscon_item_st *presences;
  jscon_integer_kt max_presences;
  jscon_integer_kt mas_members;
  jscon_char_kt *vanity_url_code;
  jscon_char_kt *description;
  jscon_char_kt *banner;
  jscon_integer_kt premium_tier;
  jscon_integer_kt premium_subscription_count;
  jscon_char_kt *preferred_locale;
  jscon_char_kt *public_updates_channel_id;
  jscon_integer_kt max_video_channel_users;
  jscon_integer_kt approximate_member_count;
  jscon_integer_kt approximate_presence_count;

  CURL *easy_handle;
} discord_guild_st;


/* USER OBJECT
https://discord.com/developers/docs/resources/user#user-object-user-structure */
typedef struct {
  jscon_char_kt *id;
  jscon_char_kt *username;
  jscon_char_kt *discriminator;
  jscon_char_kt *avatar;
  jscon_boolean_kt bot;
  jscon_boolean_kt sys;
  jscon_boolean_kt mfa_enabled;
  jscon_char_kt *locale;
  jscon_boolean_kt verified;
  jscon_char_kt *email;
  jscon_integer_kt flags;
  jscon_integer_kt premium_type;
  jscon_integer_kt public_flags;

  jscon_item_st *guilds;

  CURL *easy_handle;
} discord_user_st;

/*TODO: specify macros for lengths */
typedef struct {
  char *response;
  char bot_token[256];
  char url_route[256];
  struct curl_slist *header;
} discord_utils_st;

typedef struct {
  discord_channel_st *channel;
  discord_user_st *user;
  discord_user_st *client;
  discord_guild_st *guild;

  discord_utils_st *utils;
} discord_st;


discord_channel_st* discord_channel_init();
void discord_channel_destroy(discord_channel_st *channel);
void discord_get_channel(discord_st *discord, char channel_id[]);

discord_guild_st* discord_guild_init();
void discord_guild_destroy(discord_guild_st *guild);
void discord_get_guild(discord_st *discord, char guild_id[]);

discord_user_st* discord_user_init();
void discord_user_destroy(discord_user_st *user);
void discord_get_client(discord_st *discord);
void discord_get_user(discord_st *discord, char user_id[]);
void discord_get_client_guilds(discord_st* discord);

discord_channel_st* discord_channel_init();
void discord_channel_destroy(discord_channel_st *channel);
void discord_get_channel(discord_st *discord, char channel_id[]);

discord_guild_st* discord_guild_init();
void discord_guild_destroy(discord_guild_st *guild);
void discord_get_guild(discord_st *discord, char guild_id[]);

discord_user_st* discord_user_init();
void discord_user_destroy(discord_user_st *user);
void discord_get_client(discord_st *discord);
void discord_get_user(discord_st *discord, char user_id[]);
void discord_get_client_guilds(discord_st* discord);

discord_st* discord_init(char *bot_token);
void discord_cleanup(discord_st* discord);

#endif
