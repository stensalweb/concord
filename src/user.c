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
  if (NULL != user->id)
    free(user->id);
  if (NULL != user->username)
    free(user->username);
  if (NULL != user->discriminator)
    free(user->discriminator);
  if (NULL != user->avatar)
    free(user->avatar);
  if (NULL != user->locale)
    free(user->locale);
  if (NULL != user->email)
    free(user->email);
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

  jsonc_item_st *root = jsonc_parse(buffer.response);
  assert(NULL != root);

  jsonc_item_st *tmp;

  tmp = jsonc_get_branch(root,"id");
  user->id = jsonc_strdup(tmp);

  tmp = jsonc_get_branch(root,"username");
  user->username = jsonc_strdup(tmp);

  tmp = jsonc_get_branch(root,"discriminator");
  user->discriminator = jsonc_strdup(tmp);

  tmp = jsonc_get_branch(root,"avatar");
  user->avatar = jsonc_strdup(tmp);

  tmp = jsonc_get_branch(root,"bot");
  user->bot = jsonc_get_boolean(tmp);

  tmp = jsonc_get_branch(root,"system");
  user->sys = jsonc_get_boolean(tmp);

  tmp = jsonc_get_branch(root,"mfa_enabled");
  user->mfa_enabled = jsonc_get_boolean(tmp);

  tmp = jsonc_get_branch(root,"locale");
  user->locale = jsonc_strdup(tmp);

  tmp = jsonc_get_branch(root,"verified");
  user->verified = jsonc_get_boolean(tmp);

  tmp = jsonc_get_branch(root,"email");
  user->email = jsonc_strdup(tmp);

  tmp = jsonc_get_branch(root,"flags");
  user->flags = jsonc_get_integer(tmp);

  tmp = jsonc_get_branch(root,"premium_type");
  user->premium_type = jsonc_get_integer(tmp);

  tmp = jsonc_get_branch(root,"public_flags");
  user->public_flags = jsonc_get_integer(tmp);

  jsonc_destroy(root);
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
