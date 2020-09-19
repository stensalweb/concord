#ifndef SHAREFILE_INCLUDED
#define SHAREFILE_INCLUDED

typedef struct {
  char bot_token[256];
  char url_route[256];
} discord_config_st;

#ifdef MAIN_FILE
discord_config_st g_config;
#else
extern discord_config_st g_config;
#endif
#endif
