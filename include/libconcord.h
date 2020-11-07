#ifndef LIBCONCORD_H_
#define LIBCONCORD_H_

#include <stdbool.h>

#include <curl/curl.h>
#include <libjscon.h>
#include <uv.h>


#define CONCORD_DEBUG_MODE     1 /* set to 1 to activate debug mode */
#define CONCORD_MEMDEBUG_MODE  0 /* set to 1 to activate memdebug mode */

/* CHANNEL TYPES
https://discord.com/developers/docs/resources/channel#channel-object-channel-types */
enum discord_channel_types {
  GUILD_TEXT      = 0,
  DM              = 1,
  GUILD_VOICE     = 2,
  GROUP_DM        = 3,
  GUILD_CATEGORY  = 4,
  GUILD_NEWS      = 5,
  GUILD_STORE     = 6,
};

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
  bool nsfw;
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

  jscon_item_st *messages;
} concord_channel_st;

/* GUILD OBJECT
https://discord.com/developers/docs/resources/guild#guild-object-guild-structure */
typedef struct {
  char *id;
  char *name;
  char *icon;
  char *splash;
  char *discovery_splash;
  bool owner;
  char *owner_id;
  long long permissions;
  char *permissions_new;
  char *region;
  char *afk_channel_id;
  long long afk_timeout;
  bool embed_enabled;
  char *embed_channel_id;
  long long verification_level;
  long long default_message_notifications;
  long long explicit_content_filter;
  jscon_item_st *roles;
  jscon_item_st *emojis;
  jscon_item_st *features;
  long long mfa_level;
  char *application_id;
  bool widget_enabled;
  char *widget_channel_id;
  char *system_channel_id;
  long long system_channel_flags;
  char *rules_channel_id;
  char *joined_at;
  bool large;
  bool unavailable;
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
} concord_guild_st;


/* USER OBJECT
https://discord.com/developers/docs/resources/user#user-object-user-structure */
typedef struct {
  char *id;
  char *username;
  char *discriminator;
  char *avatar;
  bool bot;
  bool sys;
  bool mfa_enabled;
  char *locale;
  bool verified;
  char *email;
  long long flags;
  long long premium_type;
  long long public_flags;
  jscon_item_st *guilds;
} concord_user_st;

typedef struct concord_s {
  struct concord_utils_s *utils;
  struct concord_gateway_s *gateway;

  concord_channel_st *channel;
  concord_user_st *user;
  concord_user_st *client;
  concord_guild_st *guild;
} concord_st;


void concord_dispatch(concord_st *concord);
void concord_gateway_connect(concord_st *concord);

void concord_global_init();
void concord_global_cleanup();

concord_channel_st* concord_channel_init();
void concord_channel_destroy(concord_channel_st *channel);
void concord_get_channel(concord_st *concord, char channel_id[], concord_channel_st **p_channel);
void concord_get_channel_messages(concord_st *concord, char channel_id[], concord_channel_st **p_channel);

concord_guild_st* concord_guild_init();
void concord_guild_destroy(concord_guild_st *guild);
void concord_get_guild(concord_st *concord, char guild_id[], concord_guild_st **p_guild);
void concord_get_guild_channels(concord_st *concord, char guild_id[], concord_guild_st **p_guild);

concord_user_st* concord_user_init();
void concord_user_destroy(concord_user_st *user);
void concord_get_user(concord_st *concord, char user_id[], concord_user_st **p_user);
void concord_get_client(concord_st *concord, concord_user_st **p_client);
void concord_get_client_guilds(concord_st* concord, concord_user_st **p_client);

concord_st* concord_init(char token[]);
void concord_cleanup(concord_st* concord);

#endif
