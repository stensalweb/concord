
void discord_GET(discord_utils_st *utils, struct discord_clist_s *conn_list, char url_route[]);
void discord_POST(discord_utils_st *utils, struct discord_clist_s *conn_list, char url_route[]);
struct discord_clist_s* discord_get_conn(discord_utils_st *utils, char url_route[], discord_load_ft *load_cb, curl_request_ft *request_cb);
