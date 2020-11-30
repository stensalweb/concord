#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <string.h>

#include <libconcord.h>

#include "debug.h"
#include "concord-common.h"

concord_user_t*
concord_user_init()
{
  concord_user_t *new_user = safe_calloc(1, sizeof *new_user);
  new_user->id = safe_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);
  new_user->username = safe_malloc(MAX_USERNAME_LEN);
  new_user->discriminator = safe_malloc(MAX_DISCRIMINATOR_LEN);
  new_user->avatar = safe_malloc(MAX_HASH_LEN);
  new_user->locale = safe_malloc(MAX_LOCALE_LEN);
  new_user->email = safe_malloc(MAX_EMAIL_LEN);

  return new_user;
}

void
concord_user_destroy(concord_user_t *user)
{
  safe_free(user->id);
  safe_free(user->username);
  safe_free(user->discriminator);
  safe_free(user->avatar);
  safe_free(user->locale);
  safe_free(user->email);

  if (NULL != user->guilds){
    jscon_destroy(user->guilds);
  }

  safe_free(user);
}

static void
_concord_load_user(void **p_user, struct concord_response_s *response_body)
{
  concord_user_t *user = *p_user;

  jscon_scanf(response_body->str,
     "%s[id]" \
     "%s[username]" \
     "%s[discriminator]" \
     "%s[avatar]" \
     "%b[bot]" \
     "%b[system]" \
     "%b[mfa_enabled]" \
     "%s[locale]" \
     "%b[verified]" \
     "%s[email]" \
     "%d[flags]" \
     "%d[premium_type]" \
     "%d[public_flags]",
      user->id,
      user->username,
      user->discriminator,
      user->avatar,
      &user->bot,
      &user->sys,
      &user->mfa_enabled,
      user->locale,
      &user->verified,
      user->email,
      &user->flags,
      &user->premium_type,
      &user->public_flags);

  *p_user = user;
}

void
concord_get_user(concord_t *concord, char user_id[], concord_user_t **p_user)
{
  if (NULL == p_user){
    p_user = &concord->user;
  }

  /* this is a template common to every function that deals with
      sending a request to the Discord API */
  Concord_api_request( 
    concord->api,
    (void**)p_user,
    &_concord_load_user,
    GET, USERS, user_id);
}

void 
concord_get_client(concord_t *concord, concord_user_t **p_client)
{
  if (NULL == p_client){
    p_client = &concord->client;
  }

  /* this is a template common to every function that deals with
      sending a request to the Discord API */
  Concord_api_request( 
    concord->api,
    (void**)p_client,
    &_concord_load_user,
    GET, USERS, "@me");
}

static void
_concord_load_client_guilds(void **p_client, struct concord_response_s *response_body)
{
  concord_user_t *client = *p_client;

  if (NULL != client->guilds){
    jscon_destroy(client->guilds);
  }

  client->guilds = jscon_parse(response_body->str);
  DEBUG_ASSERT(NULL != client->guilds, "Out of memory");

  *p_client = client;
}

void 
concord_get_client_guilds(concord_t *concord, concord_user_t **p_client)
{
  if (NULL == p_client){
    p_client = &concord->client;
  }

  /* this is a template common to every function that deals with
      sending a request to the Discord API */
  Concord_api_request( 
    concord->api,
    (void**)p_client,
    &_concord_load_client_guilds,
    GET, USERS_GUILDS, "@me");
}
