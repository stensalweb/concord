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

  jsonc_sscanf(
      buffer.response,
      "id%s,name%s,icon%s,owner%d,permissions%lld,permissions_new%s",
      guild->id,
      guild->name,
      guild->icon,
      (int*)&guild->owner,
      &guild->permissions,
      guild->permissions_new);
 
  /* UNCOMMENT FOR TESTING
  fprintf(stdout,
      "\njson: %s\nGUILD: %s %s %s %d %lld %s\n",
      buffer.response,
      guild->id,
      guild->name,
      guild->icon,
      guild->owner,
      guild->permissions,
      guild->permissions_new);
  */
}
