#include <stdio.h>

#include "libdiscordc.h"

#define BASE_URL "https://discord.com/api"
#define MAX_URL_LENGTH 1 << 9

CURL* curl_easy_custom_init(discord_utils_st *utils);
void discord_request_get(CURL *easy_handle, discord_utils_st *utils);
void discord_request_post(CURL *easy_handle, discord_utils_st *utils);

