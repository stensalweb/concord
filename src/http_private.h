#ifndef LIBCONCORD_HTTP_H_
#define LIBCONCORD_HTTP_H_
//#include <libconcord.h> (implicit) 

enum http_method {
  NONE    = 0,
  DELETE  = 1,
  GET     = 2,
  POST    = 3,
  PATCH   = 4,
  PUT     = 5,
};

#define MAX_CONCURRENT_CONNS  15

/* ENDPOINTS */
#define CHANNELS           "/channels/%s"
#define CHANNELS_MESSAGES  "/channels/%s/messages"

#define GUILDS             "/guilds/%s"
#define GUILDS_CHANNELS    "/guilds/%s/channels"

#define USERS              "/users/%s"
#define USERS_GUILDS       "/users/%s/guilds"

/* 
  @param utils contains useful tools
  @param p_object is a pointer to the object to be loaded by load_cb
  @param load_cb is the function that will load the object attributes
    once a connection is completed
  @param http_method is the http method on the client side
    (GET, PUT, ...)
  @param endpoint is the format string that will be joined with the
    parameters to create a specific request
  @param __VAR_ARGS__ are the parameters that will be joined to the
    endpoint
*/
void Concord_http_request(
    concord_utils_st *utils,
    void **p_object,
    concord_load_obj_ft *load_cb,
    enum http_method http_method,
    char endpoint[], 
    ...);

#endif
