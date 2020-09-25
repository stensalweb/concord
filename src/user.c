#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <assert.h>
#include <string.h>

#include "REST.h"
#include "libdiscordc.h"

discord_user_st*
discord_user_init(discord_utils_st *utils)
{
  discord_user_st *new_user = calloc(1, sizeof *new_user);
  assert(NULL != new_user);

  new_user->id = calloc(1, SNOWFLAKE_INTERNAL_WORKER_ID);
  assert(NULL != new_user->id);

  new_user->username = calloc(1, USERNAME_LENGTH);
  assert(NULL != new_user->username);

  new_user->discriminator = calloc(1, DISCRIMINATOR_LENGTH);
  assert(NULL != new_user->discriminator);

  new_user->avatar = calloc(1, MAX_HASH_LENGTH);
  assert(NULL != new_user->avatar);

  new_user->locale = calloc(1, MAX_LOCALE_LENGTH);
  assert(NULL != new_user->locale);

  new_user->email = calloc(1, MAX_EMAIL_LENGTH);
  assert(NULL != new_user->email);

  new_user->easy_handle = curl_easy_custom_init(utils);
  assert(NULL != new_user->easy_handle);

  return new_user;
}

void
discord_user_destroy(discord_user_st *user)
{
  if (NULL != user->guilds)
    jsonc_destroy(user->guilds);

  free(user->id);
  free(user->username);
  free(user->discriminator);
  free(user->avatar);
  free(user->locale);
  free(user->email);

  curl_easy_cleanup(user->easy_handle);

  free(user);
}

void 
discord_get_client(discord_st* discord){
  discord_get_user(discord, "@me");
}

void
discord_get_user(discord_st* discord, char user_id[])
{
  strcpy(discord->utils->url_route, "/users/");
  strcat(discord->utils->url_route, user_id);

  // SET CURL_EASY DEFAULT CONFIG //
  discord_user_st *user = discord->user;
  discord_request_get(user->easy_handle, discord->utils);

  jsonc_scanf(
      discord->utils->response,
      "id%s,username%s,discriminator%s,avatar%s,bot%d,system%d,mfa_enabled%d,locale%s,verified%d,email%s,flags%lld,premium_type%lld,public_flags%lld",
      user->id,
      user->username,
      user->discriminator,
      user->avatar,
      (int*)&user->bot,
      (int*)&user->sys,
      (int*)&user->mfa_enabled,
      user->locale,
      (int*)&user->verified,
      user->email,
      &user->flags,
      &user->premium_type,
      &user->public_flags);

  /* UNCOMMENT FOR TESTING
  fprintf(stdout,
      "\njson: %s\nUSER: %s %s %s %s %d %d %d %s %d %s %lld %lld %lld\n",
      buffer.response,
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
}

void 
discord_get_client_guilds(discord_st *discord){
  strcpy(discord->utils->url_route, "/users/@me/guilds");

  // SET CURL_EASY DEFAULT CONFIG //
  discord_user_st *client = discord->client;
  discord_request_get(client->easy_handle, discord->utils);

  client->guilds = jsonc_parse(discord->utils->response);
  assert(NULL != client->guilds);
}
