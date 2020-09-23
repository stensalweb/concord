#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <assert.h>
#include <string.h>

#include "global_config.h"
#include "REST.h"
#include "libdiscordc.h"

discord_user_st*
discord_user_init()
{
  discord_user_st *new_user = calloc(1, sizeof *new_user);
  assert(NULL != new_user);

  new_user->easy_handle = curl_easy_init();
  assert(NULL != new_user->easy_handle);

  return new_user;
}

void
discord_user_destroy(discord_user_st *user)
{
  if (NULL != user->guilds)
    jsonc_destroy(user->guilds);

  curl_easy_cleanup(user->easy_handle);

  free(user);
}

void 
discord_get_client(discord_user_st* user){
  discord_get_user(user, "@me");
}

void
discord_get_user(discord_user_st* user, char user_id[])
{
  strcpy(g_config.url_route, "/users/");
  strcat(g_config.url_route, user_id);

  // SET CURL_EASY DEFAULT CONFIG //
  api_response_st buffer = {0};
  curl_easy_set_write(user->easy_handle, &buffer);

  jsonc_sscanf(
      buffer.response,
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
discord_get_client_guilds(discord_user_st* client){
  strcpy(g_config.url_route, "/users/@me/guilds");

  // SET CURL_EASY DEFAULT CONFIG //
  api_response_st buffer = {0};
  curl_easy_set_write(client->easy_handle, &buffer);

  client->guilds = jsonc_parse(buffer.response);
  assert(NULL != client->guilds);
}
