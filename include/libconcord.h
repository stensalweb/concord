#ifndef LIBDISCC_H_
#define LIBDISCC_H_

#include <curl/curl.h>

#include "../JSCON/include/libjscon.h"
#include "hashtable.h"

#define BASE_URL "https://discord.com/api"
#define MAX_URL_LENGTH 1 << 9

#define MAX_RESPONSE_LENGTH 1 << 15
#define MAX_HEADER_LENGTH 1 << 9
#define BOT_TOKEN_LENGTH 256

#define UTILS_HASHTABLE_SIZE 50

/* SNOWFLAKES
https://discord.com/developers/docs/reference#snowflakes */
typedef enum {
  SNOWFLAKE_INCREMENT           = 12,
  SNOWFLAKE_PROCESS_ID          = 17,
  SNOWFLAKE_INTERNAL_WORKER_ID  = 22,
  SNOWFLAKE_TIMESTAMP           = 64,
} discord_snowflake_et;

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
} discord_limits_et;

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
} discord_channel_types_et;

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
} discord_user_st;

typedef enum {
  SYNC  = 0,
  SCHEDULE = 1,
} discord_request_method_et;

struct curl_memory_s {
  char *response;
  size_t size;
};

typedef void (discord_load_ft)(void **p_object, struct curl_memory_s*);

struct discord_clist_s {
  char *primary_key; //conn_hashtable key
  char *secondary_key; //easy_hashtable key

  CURL *easy_handle; //its address will be used as secondary key for utils hashtable
  discord_load_ft *load_cb;

  struct curl_memory_s chunk;
  void **p_object; //object to be created
  struct discord_clist_s *next;
};

typedef struct discord_utils_s {
  char bot_token[256];
  struct curl_slist *header;

  hashtable_st *easy_hashtable;
  CURLM *multi_handle;
  hashtable_st *conn_hashtable;
  struct discord_clist_s *conn_list;

  discord_request_method_et method;
  void (*method_cb)(struct discord_utils_s *utils, struct discord_clist_s *conn);
} discord_utils_st;

typedef void (curl_request_ft)(discord_utils_st *utils, struct discord_clist_s *conn, char url_route[]);

typedef struct discord_s {
  discord_channel_st *channel;
  discord_user_st *user;
  discord_user_st *client;
  discord_guild_st *guild;

  discord_utils_st *utils;
} discord_st;


void __discord_free(void **p_ptr);
#define discord_free(n) __discord_free((void**)&n)
void* __discord_malloc(size_t size, unsigned long line);
#define discord_malloc(n) __discord_malloc(n, __LINE__)

void discord_global_init();
void discord_global_cleanup();

void discord_request_method(discord_st *discord, discord_request_method_et method);
void discord_GET(discord_utils_st *utils, struct discord_clist_s *conn_list, char url_route[]);
void discord_POST(discord_utils_st *utils, struct discord_clist_s *conn_list, char url_route[]);
struct discord_clist_s* discord_get_conn(discord_utils_st *utils, char url_route[], discord_load_ft *load_cb, curl_request_ft *request_cb);
void discord_dispatch(discord_st *discord);

discord_channel_st* discord_channel_init();
void discord_channel_destroy(discord_channel_st *channel);
void discord_get_channel(discord_st *discord, char channel_id[], discord_channel_st **p_channel);

discord_guild_st* discord_guild_init();
void discord_guild_destroy(discord_guild_st *guild);
void discord_get_guild(discord_st *discord, char guild_id[], discord_guild_st **p_guild);
void discord_get_guild_channels(discord_st *discord, char guild_id[], discord_guild_st **p_guild);

discord_user_st* discord_user_init();
void discord_user_destroy(discord_user_st *user);
void discord_get_client(discord_st *discord, discord_user_st **p_client);
void discord_get_user(discord_st *discord, char user_id[], discord_user_st **p_user);
void discord_get_client_guilds(discord_st* discord, discord_user_st **p_client);

discord_st* discord_init(char *bot_token);
void discord_cleanup(discord_st* discord);

#endif
