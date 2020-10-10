#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <assert.h>
#include <string.h>

#include "libconcord.h"

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

  new_user->hashtable = hashtable_init();
  hashtable_build(new_user->hashtable, CLIST_HASHTABLE_SIZE);

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

  hashtable_destroy(user->hashtable);
  discord_clist_free_all(user->conn_list);

  discord_free(user);
}

static void
_discord_ld_user(void *ptr, struct curl_memory_s *chunk)
{
  discord_user_st *user = ptr;

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

  chunk->size = 0;
  discord_free(chunk->response);
}

void 
discord_get_client(discord_st* discord){
  discord_get_user(discord, "@me");
}

void
discord_get_user(discord_st* discord, char user_id[])
{
  char url_route[256] = "/users/";
  strcat(url_route, user_id);

  discord_user_st *user = discord->user;
  struct discord_clist_s *conn = discord_get_conn(
                                    discord->utils,
                                    "GetUser",
                                    user->hashtable,
                                    &user->conn_list,
                                    &_discord_ld_user);

  discord_request_get(discord->utils, conn, url_route);

  if (SYNC == discord->utils->method){
    (*conn->load_cb)(user, &conn->chunk);
  }
}

static void
_discord_ld_client_guilds(void *ptr, struct curl_memory_s *chunk)
{
  discord_user_st *client = ptr;

  if (NULL != client->guilds){
    jscon_destroy(client->guilds);
  }

  client->guilds = jscon_parse(chunk->response);

  chunk->size = 0;
  discord_free(chunk->response);
}

void 
discord_get_client_guilds(discord_st *discord)
{
  char url_route[256] = "/users/@me/guilds";

  discord_user_st *client = discord->client;
  struct discord_clist_s *conn = discord_get_conn(
                                    discord->utils,
                                    "GetClientGuilds",
                                    client->hashtable,
                                    &client->conn_list,
                                    &_discord_ld_client_guilds);

  discord_request_get(discord->utils, conn, url_route);

  if (SYNC == discord->utils->method){
    (*conn->load_cb)(client, &conn->chunk);
  }
}
