#include <stdio.h>


#define BASE_URL "https://discord.com/api"
#define MAX_URL_LENGTH 1 << 9
#define MAX_HEADER_LENGTH 1 << 9
#define MAX_RESPONSE_LENGTH 1 << 15


typedef struct {
  char response[MAX_RESPONSE_LENGTH];
} api_response_st;


void curl_easy_set_write(CURL *easy_handle, api_response_st *buffer);
size_t api_response_write_callback(char *content, size_t size, size_t nmemb, void *p_userdata);

