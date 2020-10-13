#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "libconcord.h"

int main(void)
{
  FILE *f_bot_token = fopen("bot_token","rb");
  char bot_token[100];
  fgets(bot_token, 99, f_bot_token);
  fclose(f_bot_token);

  concord_global_init();

  concord_st *concord = concord_init(bot_token); //scheduler
  concord_request_method(concord, SCHEDULE);

  concord_user_st *client = concord_user_init();
  concord_get_client_guilds(concord, &client);
  concord_dispatch(concord->utils);

  /* THIS WILL FETCH CHANNELS FROM EACH GUILD BOT IS A PART OF */
  concord_guild_st **guilds = concord_malloc(jscon_size(client->guilds) * sizeof *guilds);
  for (long i=0; i < jscon_size(client->guilds); ++i){
    jscon_item_st *item_guild = jscon_get_byindex(client->guilds, i);
    char *guild_id = jscon_get_string(jscon_get_branch(item_guild, "id"));
    assert(NULL != guild_id);

    guilds[i] = concord_guild_init();
    concord_get_guild(concord, guild_id, guilds+i);
    concord_get_guild_channels(concord, guild_id, guilds+i);
  }
  concord_dispatch(concord->utils);


  // FETCH 50 MESSAGES FROM EACH CHANNEL FROM FIRST GUILD AND WILL OUTPUT THEM TO A a.out FILE
  concord_channel_st **channels = concord_malloc(jscon_size(guilds[0]->channels) * sizeof *channels);

  char *buffer;
  for (long i=0; i < jscon_size(guilds[0]->channels); ++i){
    jscon_item_st *item_channel = jscon_get_byindex(guilds[0]->channels, i);
    char *channel_id = jscon_get_string(jscon_get_branch(item_channel, "id"));
    assert(NULL != channel_id);

    channels[i] = concord_channel_init();
    concord_get_channel_messages(concord, channel_id, channels+i);
  }
  concord_dispatch(concord->utils);

  FILE *f_out = fopen("a.out", "w");
  assert(NULL != f_out);

  for (long i=0; i < jscon_size(guilds[0]->channels); ++i){
    if (NULL == channels[i]->messages)
      continue;

    buffer = jscon_stringify(channels[i]->messages, JSCON_ANY);
    fprintf(f_out, "%s\n", buffer);
    free(buffer);
  }
  fclose(f_out);

  // DELETE ALLOCATED OBJECTS
  for (long i=0; i < jscon_size(guilds[0]->channels); ++i){
    concord_channel_destroy(channels[i]);
  }
  concord_free(channels);
 
  for (long i=0; i < jscon_size(client->guilds); ++i){
    concord_guild_destroy(guilds[i]);
  }
  concord_free(guilds);

  concord_user_destroy(client);

  concord_cleanup(concord);
  concord_global_cleanup();
}
