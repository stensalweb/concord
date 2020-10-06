#include <stdio.h>

#include "libconcord.h"

#define BASE_URL "https://discord.com/api"
#define MAX_URL_LENGTH 1 << 9

CURL* curl_easy_custom_init(discord_utils_st *utils);
char* discord_request_get(CURL *easy_handle, char url_route[]);
char* discord_request_post(CURL *easy_handle, char url_route[]);

