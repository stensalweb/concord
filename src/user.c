#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <assert.h>
#include <string.h>

#include "libconcord.h"
#include "api_wrapper_private.h"

discord_user_st*
discord_user_init(discord_utils_st *utils)
{
  discord_user_st *new_user = discord_malloc(sizeof *new_user);
  new_user->id = discord_malloc(SNOWFLAKE_INTERNAL_WORKER_ID);
  new_user->username = discord_malloc(USERNAME_LENGTH);
  new_user->discriminator = discord_malloc(DISCRIMINATOR_LENGTH);
  new_user->avatar = discord_malloc(MAX_HASH_LENGTH);
  new_user->locale = discord_malloc(MAX_LOCALE_LENGTH);
  new_user->email = discord_malloc(MAX_EMAIL_LENGTH);

  return new_user;
}

void
discord_user_destroy(discord_user_st *user)
{
  discord_free(user->id);
  discord_free(user->username);
  discord_free(user->discriminator);
  discord_free(user->avatar);
  discord_free(user->locale);
  discord_free(user->email);

  if (NULL != user->guilds){
    jscon_destroy(user->guilds);
  }

  discord_free(user);
}

static void
_discord_ld_user(void **p_user, struct curl_memory_s *chunk)
{
  discord_user_st *user = *p_user;

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
  discord_free(chunk->response);
}

void
discord_get_user(discord_st *discord, char user_id[], discord_user_st **p_user)
{
  char url_route[256] = "/users/";
  strcat(url_route, user_id);

  if (NULL == p_user){
    p_user = &discord->user;
  }

  struct discord_clist_s *conn = discord_get_conn(
                                    discord->utils,
                                    url_route,
                                    &_discord_ld_user,
                                    &discord_GET);

  conn->p_object = (void**)p_user;

  (*discord->utils->method_cb)(discord->utils, conn);

  if (SYNC == discord->utils->method){
    _discord_ld_user((void**)p_user, &conn->chunk);
  }
}

static void
_discord_ld_client(void **p_client, struct curl_memory_s *chunk)
{
  discord_user_st *client = *p_client;

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
  discord_free(chunk->response);
}

void 
discord_get_client(discord_st *discord, discord_user_st **p_client)
{
  char url_route[256] = "/users/@me";

  if (NULL == p_client){
    p_client = &discord->client;
  }

  struct discord_clist_s *conn = discord_get_conn(
                                    discord->utils,
                                    url_route,
                                    &_discord_ld_client,
                                    &discord_GET);

  conn->p_object = (void**)p_client;

  (*discord->utils->method_cb)(discord->utils, conn);

  if (SYNC == discord->utils->method){
    _discord_ld_client((void**)p_client, &conn->chunk);
  }
}

static void
_discord_ld_client_guilds(void **p_client, struct curl_memory_s *chunk)
{
  discord_user_st *client = *p_client;

  if (NULL != client->guilds){
    jscon_destroy(client->guilds);
  }

  client->guilds = jscon_parse(chunk->response);

  *p_client = client;

  chunk->size = 0;
  discord_free(chunk->response);
}

void 
discord_get_client_guilds(discord_st *discord, discord_user_st **p_client)
{
  char url_route[256] = "/users/@me/guilds";

  if (NULL == p_client){
    p_client = &discord->client;
  }

  struct discord_clist_s *conn = discord_get_conn(
                                    discord->utils,
                                    url_route,
                                    &_discord_ld_client_guilds,
                                    &discord_GET);

  conn->p_object = (void**)p_client;

  (*discord->utils->method_cb)(discord->utils, conn);

  if (SYNC == discord->utils->method){
    _discord_ld_client_guilds((void**)p_client, &conn->chunk);
  }
}
