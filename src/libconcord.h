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
  char *id;
  long long type;
  char *guild_id;
  long long position;
  jscon_item_st *permission_overwrites;
  char *name;
  char *topic;
  _Bool nsfw;
  char *last_message_id;
  long long bitrate;
  long long user_limit;
  long long rate_limit_per_user;
  jscon_item_st *recipients;
  char *icon;
  char *owner_id;
  char *application_id;
  char *parent_id;
  char *last_pin_timestamp;

  CURL *easy_handle;
} discord_channel_st;

/* GUILD OBJECT
https://discord.com/developers/docs/resources/guild#guild-object-guild-structure */
typedef struct {
  char *id;
  char *name;
  char *icon;
  char *splash;
  char *discovery_splash;
  _Bool owner;
  char *owner_id;
  long long permissions;
  char *permissions_new;
  char *region;
  char *afk_channel_id;
  long long afk_timeout;
  _Bool embed_enabled;
  char *embed_channel_id;
  long long verification_level;
  long long default_message_notifications;
  long long explicit_content_filter;
  jscon_item_st *roles;
  jscon_item_st *emojis;
  jscon_item_st *features;
  long long mfa_level;
  char *application_id;
  _Bool widget_enabled;
  char *widget_channel_id;
  char *system_channel_id;
  long long system_channel_flags;
  char *rules_channel_id;
  char *joined_at;
  _Bool large;
  _Bool unavailable;
  long long member_count;
  jscon_item_st *voice_states;
  jscon_item_st *members;
  jscon_item_st *channels;
  jscon_item_st *presences;
  long long max_presences;
  long long mas_members;
  char *vanity_url_code;
  char *description;
  char *banner;
  long long premium_tier;
  long long premium_subscription_count;
  char *preferred_locale;
  char *public_updates_channel_id;
  long long max_video_channel_users;
  long long approximate_member_count;
  long long approximate_presence_count;

  CURL *easy_handle;
} discord_guild_st;


/* USER OBJECT
https://discord.com/developers/docs/resources/user#user-object-user-structure */
typedef struct {
  char *id;
  char *username;
  char *discriminator;
  char *avatar;
  _Bool bot;
  _Bool sys;
  _Bool mfa_enabled;
  char *locale;
  _Bool verified;
  char *email;
  long long flags;
  long long premium_type;
  long long public_flags;

  jscon_item_st *guilds;

  CURL *easy_handle;
} discord_user_st;

typedef struct {
  char bot_token[256];
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
