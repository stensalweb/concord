#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <assert.h>
#include <string.h>

#include "httpclient.h"
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

  new_user->easy_handle = curl_easy_custom_init(utils);

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

  curl_easy_cleanup(user->easy_handle);

  discord_free(user);
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

  // SET CURL_EASY DEFAULT CONFIG //
  discord_user_st *user = discord->user;
  char *response = discord_request_get(discord, user->easy_handle, url_route);

  jscon_scanf(response,
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

  discord_free(response);
}

void 
discord_get_client_guilds(discord_st *discord){
  char url_route[256] = "/users/@me/guilds";

  // SET CURL_EASY DEFAULT CONFIG //
  discord_user_st *client = discord->client;
  char *response = discord_request_get(discord, client->easy_handle, url_route);

  client->guilds = jscon_parse(response);

  discord_free(response);
}
