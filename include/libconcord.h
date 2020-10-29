#ifndef LIBCONCORD_H_
#define LIBCONCORD_H_

#include <stdbool.h>

#include <curl/curl.h>
#include <libjscon.h>
#include <uv.h>

#define CONCORD_DEBUG_MODE  1 //set to 1 to activate debug mode

#define BASE_URL "https://discord.com/api"

#define MAX_URL_LENGTH       1 << 9
#define MAX_RESPONSE_LENGTH  1 << 15
#define MAX_HEADER_LENGTH    1 << 9

#define ENDPOINT_LENGTH  256

#define UTILS_HASHTABLE_SIZE  50

/* HTTP RESPONSE CODES
https://discord.com/developers/docs/topics/opcodes-and-status-codes#http-http-response-codes */
enum http_response_code {
  CURL_NO_RESPONSE              = 0,

  DISCORD_OK                    = 200,
  DISCORD_CREATED               = 201,
  DISCORD_NO_CONTENT            = 204,
  DISCORD_NOT_MODIFIED          = 304,
  DISCORD_BAD_REQUEST           = 400,
  DISCORD_UNAUTHORIZED          = 401,
  DISCORD_FORBIDDEN             = 403,
  DISCORD_NOT_FOUND             = 404,
  DISCORD_METHOD_NOT_ALLOWED    = 405,
  DISCORD_TOO_MANY_REQUESTS     = 429,
  DISCORD_GATEWAY_UNAVAILABLE   = 502,
};

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
  GUILD_TEXT      = 0,
  DM              = 1,
  GUILD_VOICE     = 2,
  GROUP_DM        = 3,
  GUILD_CATEGORY  = 4,
  GUILD_NEWS      = 5,
  GUILD_STORE     = 6,
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

struct curl_response_s {
  char *str;
  size_t size;
};

typedef void (concord_load_obj_ft)(void **p_object, struct curl_response_s *response_body);

typedef struct concord_context_s {
  uv_poll_t poll_handle;
  curl_socket_t sockfd;
} concord_context_st;

struct concord_bucket_s; //forward declaration

struct concord_conn_s {
  concord_context_st *context;
  CURL *easy_handle; //easy handle used to perform the request

  struct curl_response_s response_body; //stores response body associated with the easy_handle

  void **p_object; //hold onto object to be passed as a load_cb parameter
  concord_load_obj_ft *load_cb; //object load callback

  struct concord_bucket_s *p_bucket; //bucket this connection node is a part of
};

struct concord_utils_s; //forward declaration

struct concord_bucket_s {
  struct concord_utils_s *p_utils;

  uv_timer_t timer;
  int remaining;

  char *hash_key;

  struct concord_conn_s **queue;
  size_t num_conn;

  size_t bottom;
  size_t top;
};

typedef struct concord_utils_s {
  struct curl_slist *request_header; /* the default request header sent to discord servers */

  struct dictionary_s *header; /* this holds the http response header */

  uv_loop_t *loop;
  CURLM *multi_handle;
  int transfers_onhold;
  int transfers_running;
  uv_timer_t timeout;

  struct concord_bucket_s **client_buckets;
  size_t num_buckets;

  struct dictionary_s *bucket_dict; //get buckets by their endpoints/major parameters

  /* get connection nodes by their easy_handle unique address
      @todo this will be rendered useless I switch to sockets */
  struct dictionary_s *easy_dict;

  char *token; /* @todo hash/unhash token */
} concord_utils_st;


typedef struct concord_s {
  /* because of alignment, utils can be expanded to the concord_st its a part of
      concord == (concord_st)concord->utils */
  concord_utils_st utils;

  concord_channel_st *channel;
  concord_user_st *user;
  concord_user_st *client;
  concord_guild_st *guild;
} concord_st;


void concord_dispatch(concord_st *concord);

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
