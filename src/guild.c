#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <assert.h>
#include <string.h>

#include "global_config.h"
#include "REST.h"
#include "libdiscordc.h"

discord_guild_st*
discord_guild_init()
{
  discord_guild_st *new_guild = calloc(1, sizeof *new_guild);
  assert(NULL != new_guild);

  new_guild->easy_handle = curl_easy_init();
  assert(NULL != new_guild->easy_handle);

  return new_guild;
}

void
discord_guild_destroy(discord_guild_st *guild)
{
  if(NULL != guild->id)
    free(guild->id);
  if(NULL != guild->name)
    free(guild->name);
  if(NULL != guild->icon)
    free(guild->icon);
  if(NULL != guild->permissions_new)
    free(guild->permissions_new);

  //@todo: add remaining members
  curl_easy_cleanup(guild->easy_handle);

  free(guild);
}

void
discord_get_guild(discord_guild_st* guild, char guild_id[])
{
  strcpy(g_config.url_route, "/guilds/");
  strcat(g_config.url_route, guild_id);

  // SET CURL_EASY DEFAULT CONFIG //
  api_response_st buffer = {0};
  curl_easy_set_write(guild->easy_handle, &buffer);

  jsonc_item_st *root = jsonc_parse(buffer.response);
  assert(NULL != root);

  jsonc_item_st *tmp;

  tmp = jsonc_get_branch(root,"id");
  guild->id = jsonc_strdup(tmp);

  tmp = jsonc_get_branch(root,"name");
  guild->name = jsonc_strdup(tmp);

  tmp = jsonc_get_branch(root,"icon");
  guild->icon = jsonc_strdup(tmp);

  tmp = jsonc_get_branch(root,"owner");
  guild->owner = jsonc_get_boolean(tmp);

  tmp = jsonc_get_branch(root,"permissions");
  guild->permissions = jsonc_get_integer(tmp);

  tmp = jsonc_get_branch(root,"permissions_new");
  guild->permissions_new = jsonc_strdup(tmp);

  jsonc_destroy(root);
}
