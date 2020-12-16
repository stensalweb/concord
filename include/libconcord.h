#ifndef LIBCONCORD_H_
#define LIBCONCORD_H_

#include <stdbool.h>

#include <curl/curl.h>
#include <libjscon.h>
#include <uv.h>


#define DEBUG_MODE     1 /* set to 1 to activate debug mode */
#define MEMDEBUG_MODE  0 /* set to 1 to activate memdebug mode */

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
    int type;
    char *guild_id;
    int position;
    jscon_item_t *permission_overwrites;
    char *name;
    char *topic;
    bool nsfw;
    char *last_message_id;
    int bitrate;
    int user_limit;
    int rate_limit_per_user;
    jscon_item_t *recipients;
    char *icon;
    char *owner_id;
    char *application_id;
    char *parent_id;
    char *last_pin_timestamp;

    jscon_item_t *messages;
} concord_channel_t;

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
    int permissions;
    char *permissions_new;
    char *region;
    char *afk_channel_id;
    int afk_timeout;
    bool embed_enabled;
    char *embed_channel_id;
    int verification_level;
    int default_message_notifications;
    int explicit_content_filter;
    jscon_item_t *roles;
    jscon_item_t *emojis;
    jscon_item_t *features;
    int mfa_level;
    char *application_id;
    bool widget_enabled;
    char *widget_channel_id;
    char *system_channel_id;
    int system_channel_flags;
    char *rules_channel_id;
    char *joined_at;
    bool large;
    bool unavailable;
    int member_count;
    jscon_item_t *voice_states;
    jscon_item_t *members;
    jscon_item_t *channels;
    jscon_item_t *presences;
    int max_presences;
    int mas_members;
    char *vanity_url_code;
    char *description;
    char *banner;
    int premium_tier;
    int premium_subscription_count;
    char *preferred_locale;
    char *public_updates_channel_id;
    int max_video_channel_users;
    int approximate_member_count;
    int approximate_presence_count;
} concord_guild_t;


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
    int flags;
    int premium_type;
    int public_flags;
    jscon_item_t *guilds;
} concord_user_t;

typedef struct concord_s {
    struct concord_api_s *api;
    struct concord_ws_s *ws;

    concord_channel_t *channel;
    concord_user_t *user;
    concord_user_t *client;
    concord_guild_t *guild;
} concord_t;


void concord_dispatch(concord_t *concord);
void concord_ws_connect(concord_t *concord);
void concord_ws_disconnect(concord_t *concord);

int concord_ws_isrunning(concord_t *concord);

void concord_global_init();
void concord_global_cleanup();

concord_channel_t* concord_channel_init();
void concord_channel_destroy(concord_channel_t *channel);
void concord_get_channel(concord_t *concord, char channel_id[], concord_channel_t **p_channel);
void concord_get_channel_messages(concord_t *concord, char channel_id[], concord_channel_t **p_channel);

concord_guild_t* concord_guild_init();
void concord_guild_destroy(concord_guild_t *guild);
void concord_get_guild(concord_t *concord, char guild_id[], concord_guild_t **p_guild);
void concord_get_guild_channels(concord_t *concord, char guild_id[], concord_guild_t **p_guild);

concord_user_t* concord_user_init();
void concord_user_destroy(concord_user_t *user);
void concord_get_user(concord_t *concord, char user_id[], concord_user_t **p_user);
void concord_get_client(concord_t *concord, concord_user_t **p_client);
void concord_get_client_guilds(concord_t* concord, concord_user_t **p_client);

concord_t* concord_init(char token[]);
void concord_cleanup(concord_t* concord);

#endif
