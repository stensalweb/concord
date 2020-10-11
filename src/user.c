#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <assert.h>
#include <string.h>

#include "libconcord.h"
#include "api_wrapper_private.h"

concord_user_st*
concord_user_init(concord_utils_st *utils)
{
  concord_user_st *new_user = concord_malloc(sizeof *new_user);
  new_user->id = concord_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);
  new_user->username = concord_malloc(USERNAME_LENGTH);
  new_user->discriminator = concord_malloc(DISCRIMINATOR_LENGTH);
  new_user->avatar = concord_malloc(MAX_HASH_LENGTH);
  new_user->locale = concord_malloc(MAX_LOCALE_LENGTH);
  new_user->email = concord_malloc(MAX_EMAIL_LENGTH);

  return new_user;
}

void
concord_user_destroy(concord_user_st *user)
{
  concord_free(user->id);
  concord_free(user->username);
  concord_free(user->discriminator);
  concord_free(user->avatar);
  concord_free(user->locale);
  concord_free(user->email);

  if (NULL != user->guilds){
    jscon_destroy(user->guilds);
  }

  concord_free(user);
}

static void
_concord_ld_user(void **p_user, struct curl_memory_s *chunk)
{
  concord_user_st *user = *p_user;

  jscon_scanf(chunk->response,
     "#id%js \
      #username%js \
      #discriminator%js \
      #avatar%js \
      #bot%jb \
      #system%jb \
      #mfa_enabled%jb \
      #locale%js \
      #verified%jb \
      #email%js \
      #flags%jd \
      #premium_type%jd \
      #public_flags%jd",
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

  /* UNCOMMENT FOR TESTING
  fprintf(stdout,
      "\njson: %s\nUSER: %s %s %s %s %d %d %d %s %d %s %lld %lld %lld\n",
      response,
      user->id,
      user->username,
      user->discriminator,
      user->avatar,
      user->bot,
      user->sys,
      user->mfa_enabled,
      user->locale,
      user->verified,
      user->email,
      user->flags,
      user->premium_type,
      user->public_flags);
  */

  *p_user = user;

  chunk->size = 0;
  concord_free(chunk->response);
}

void
concord_get_user(concord_st *concord, char user_id[], concord_user_st **p_user)
{
  char endpoint[ENDPOINT_LENGTH] = "/users/";
  strcat(endpoint, user_id);

  if (NULL == p_user){
    p_user = &concord->user;
  }

  /* this is a template common to every function that deals with
      sending a request to the Discord API */
  Concord_request_perform( 
    concord->utils,
    (void**)p_user,
    "GetUser",
    endpoint,
    &_concord_ld_user,
    &Concord_GET);
}

static void
_concord_ld_client(void **p_client, struct curl_memory_s *chunk)
{
  concord_user_st *client = *p_client;

  jscon_scanf(chunk->response,
     "#id%js \
      #username%js \
      #discriminator%js \
      #avatar%js \
      #bot%jb \
      #system%jb \
      #mfa_enabled%jb \
      #locale%js \
      #verified%jb \
      #email%js \
      #flags%jd \
      #premium_type%jd \
      #public_flags%jd",
      client->id,
      client->username,
      client->discriminator,
      client->avatar,
      &client->bot,
      &client->sys,
      &client->mfa_enabled,
      client->locale,
      &client->verified,
      client->email,
      &client->flags,
      &client->premium_type,
      &client->public_flags);

  /* UNCOMMENT FOR TESTING
  fprintf(stdout,
      "\njson: %s\nCLIENT: %s %s %s %s %d %d %d %s %d %s %lld %lld %lld\n",
      response,
      client->id,
      client->username,
      client->discriminator,
      client->avatar,
      client->bot,
      client->sys,
      client->mfa_enabled,
      client->locale,
      client->verified,
      client->email,
      client->flags,
      client->premium_type,
      client->public_flags);
  */

  *p_client = client;

  chunk->size = 0;
  concord_free(chunk->response);
}

void 
concord_get_client(concord_st *concord, concord_user_st **p_client)
{
  char endpoint[] = "/users/@me";

  if (NULL == p_client){
    p_client = &concord->client;
  }

  /* this is a template common to every function that deals with
      sending a request to the Discord API */
  Concord_request_perform( 
    concord->utils,
    (void**)p_client,
    "GetClient",
    endpoint,
    &_concord_ld_client,
    &Concord_GET);
}

static void
_concord_ld_client_guilds(void **p_client, struct curl_memory_s *chunk)
{
  concord_user_st *client = *p_client;

  if (NULL != client->guilds){
    jscon_destroy(client->guilds);
  }

  client->guilds = jscon_parse(chunk->response);

  *p_client = client;

  chunk->size = 0;
  concord_free(chunk->response);
}

void 
concord_get_client_guilds(concord_st *concord, concord_user_st **p_client)
{
  char endpoint[] = "/users/@me/guilds";

  if (NULL == p_client){
    p_client = &concord->client;
  }

  /* this is a template common to every function that deals with
      sending a request to the Discord API */
  Concord_request_perform( 
    concord->utils,
    (void**)p_client,
    "GetClientGuilds",
    endpoint,
    &_concord_ld_client_guilds,
    &Concord_GET);
}
