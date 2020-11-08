#include <stdio.h>
#include <stdlib.h>

#include <libconcord.h>
#include "concord-common.h"

#include "hashtable.h"
#include "debug.h"


concord_gateway_st*
Concord_gateway_init(char token[])
{
  concord_gateway_st *new_gateway = safe_malloc(sizeof *new_gateway);
  
  new_gateway->loop = safe_malloc(sizeof *new_gateway->loop);
  uv_loop_init(new_gateway->loop);
  uv_loop_set_data(new_gateway->loop, new_gateway);

  new_gateway->token = strndup(token, strlen(token)-1);
  DEBUG_ASSERT(NULL != new_gateway->token, "Out of memory");

  uv_timer_init(new_gateway->loop, &new_gateway->timeout);
  uv_handle_set_data((uv_handle_t*)&new_gateway->timeout, new_gateway);

  uv_timer_init(new_gateway->loop, &new_gateway->heartbeat_signal);
  uv_handle_set_data((uv_handle_t*)&new_gateway->heartbeat_signal, new_gateway);

  new_gateway->easy_handle = Concord_gateway_easy_init(new_gateway);
  new_gateway->multi_handle = Concord_gateway_multi_init(new_gateway);

  return new_gateway;
}

static void
_uv_on_walk_cb(uv_handle_t *handle, void *arg)
{
  uv_close(handle, NULL);

  (void)arg;
}

static void
_concord_gateway_disconnect(concord_gateway_st *gateway)
{
  uv_async_send(&gateway->async);
  uv_thread_join(&gateway->thread_id);
}

void
Concord_gateway_destroy(concord_gateway_st *gateway)
{
  if (RUNNING == gateway->status){
    _concord_gateway_disconnect(gateway);
  }

  if (gateway->context){
    Concord_context_destroy(gateway->context);
    gateway->context = NULL;
  }

  curl_multi_cleanup(gateway->multi_handle);

  cws_free(gateway->easy_handle);

  int uvcode = uv_loop_close(gateway->loop);
  if (UV_EBUSY == uvcode){ //there are still handles that need to be closed
    uv_walk(gateway->loop, &_uv_on_walk_cb, NULL); //close each handle encountered

    uvcode = uv_run(gateway->loop, UV_RUN_DEFAULT); //run the loop again to close remaining handles
    DEBUG_ASSERT(!uvcode, uv_strerror(uvcode));

    uvcode = uv_loop_close(gateway->loop); //finally, close the loop
    DEBUG_ASSERT(!uvcode, uv_strerror(uvcode));
  }
  safe_free(gateway->loop);

  if (gateway->event_data){
    jscon_destroy(gateway->event_data);
  }

  safe_free(gateway->token);

  safe_free(gateway); 
}

static void
_uv_perform_cb(uv_poll_t *req, int uvstatus, int events)
{
  DEBUG_ASSERT(!uvstatus, uv_strerror(uvstatus));

  concord_gateway_st *gateway = uv_handle_get_data((uv_handle_t*)req);

  int flags = 0;
  if (events & UV_READABLE) flags |= CURL_CSELECT_IN;
  if (events & UV_WRITABLE) flags |= CURL_CSELECT_OUT;

  CURLMcode mcode = curl_multi_socket_action(gateway->multi_handle, gateway->context->sockfd, flags, &gateway->transfers_running);
  DEBUG_ASSERT(CURLM_OK == mcode, curl_multi_strerror(mcode));


  CURLMsg *msg; /* for picking up messages with the transfer status */
  int pending; /*how many messages are left */

  while ((msg = curl_multi_info_read(gateway->multi_handle, &pending)))
  {
    if (CURLMSG_DONE != msg->msg)
      continue;
    
    DEBUG_PRINT("HTTP completed with status %d '%s'", msg->data.result, curl_easy_strerror(msg->data.result));

    gateway->status = INNACTIVE;
    uv_stop(gateway->loop);
  }
}

static void
_uv_on_timeout_cb(uv_timer_t *req)
{
  concord_gateway_st *gateway = uv_handle_get_data((uv_handle_t*)req);

  CURLMcode mcode = curl_multi_socket_action(gateway->multi_handle, CURL_SOCKET_TIMEOUT, 0, &gateway->transfers_running);
  DEBUG_ASSERT(CURLM_OK == mcode, curl_multi_strerror(mcode));
}

int
Concord_gateway_timeout_cb(CURLM *multi_handle, long timeout_ms, void *p_userdata)
{
  DEBUG_PRINT("TIMEOUT_MS: %ld", timeout_ms);
  uv_timer_t *timeout = p_userdata;
  int uvcode;


  if (timeout_ms < 0){
    uvcode = uv_timer_stop(timeout);
    DEBUG_ASSERT(!uvcode, uv_strerror(uvcode));
  } else {
    if (0 == timeout_ms){
      timeout_ms = 1;
    }

    uvcode = uv_timer_start(timeout, &_uv_on_timeout_cb, timeout_ms, 0);
    DEBUG_ASSERT(!uvcode, uv_strerror(uvcode));
  }

  (void)multi_handle;

  return 0;
}

int
Concord_gateway_socket_cb(CURL *easy_handle, curl_socket_t sockfd, int action, void *p_userdata, void *p_socket)
{
  concord_gateway_st *gateway = p_userdata;
  int uvcode;
  int events = 0;


  switch (action){
  case CURL_POLL_IN:
  case CURL_POLL_OUT:
  case CURL_POLL_INOUT:
      if (!gateway->context){
        gateway->context = Concord_context_init(gateway->loop, sockfd);
      }

      if (action != CURL_POLL_IN) events |= UV_WRITABLE;
      if (action != CURL_POLL_OUT) events |= UV_READABLE;

      uvcode = uv_poll_start(&gateway->context->poll_handle, events, &_uv_perform_cb);
      DEBUG_ASSERT(!uvcode, uv_strerror(uvcode));
      break;
  case CURL_POLL_REMOVE:
      if (gateway->context){
        uvcode = uv_poll_stop(&gateway->context->poll_handle);
        DEBUG_ASSERT(!uvcode, uv_strerror(uvcode));

        Concord_context_destroy(gateway->context);
        gateway->context = NULL;
      }
      break;
  default:
      DEBUG_PRINT("Unknown CURL_POLL_XXX option encountered\n\tCode: %d", action);
      abort();
  }

  (void)easy_handle;
  (void)p_socket;

  return 0;
}

void
Concord_on_connect_cb(void *data, CURL *easy_handle, const char *ws_protocols)
{
  DEBUG_PRINT("Connected, WS-Protocols: '%s'", ws_protocols);

  (void)easy_handle;
  (void)data;
}

static void
_concord_heartbeat_send(concord_gateway_st *gateway)
{
  char send_payload[250];

  if (0 == gateway->seq_number){
    snprintf(send_payload, 249, "{\"op\": 1, \"d\": null}");
  } else {
    snprintf(send_payload, 249, "{\"op\": 1, \"d\": %d}", gateway->seq_number);
  }

  DEBUG_PRINT("HEARTBEAT_PAYLOAD:\n\t\t%s", send_payload);
  bool ret = cws_send_text(gateway->easy_handle, send_payload);
  DEBUG_ASSERT(true == ret, "Couldn't send heartbeat payload");

  _uv_perform_cb(&gateway->context->poll_handle, uv_is_closing((uv_handle_t*)&gateway->context->poll_handle), UV_READABLE);
}

static void
_uv_on_heartbeat_signal_cb(uv_timer_t *req)
{
  concord_gateway_st *gateway = uv_handle_get_data((uv_handle_t*)req);

  DEBUG_PRINT("REPEAT_MS: %ld", uv_timer_get_repeat(&gateway->heartbeat_signal));

  _concord_heartbeat_send(gateway);
}

char*
_concord_gateway_strevent(enum gateway_opcode opcode)
{
  switch(opcode){
  case GATEWAY_DISPATCH:
        return "GATEWAY_DISPATCH";
  case GATEWAY_HEARTBEAT:
        return "GATEWAY_HEARTBEAT";
  case GATEWAY_IDENTIFY:
        return "GATEWAY_IDENTIFY";
  case GATEWAY_PRESENCE_UPDATE:
        return "GATEWAY_PRESENCE_UPDATE";
  case GATEWAY_VOICE_STATE_UPDATE:
        return "GATEWAY_VOICE_STATE_UPDATE";
  case GATEWAY_RESUME:
        return "GATEWAY_RESUME";
  case GATEWAY_RECONNECT:
        return "GATEWAY_RECONNECT";
  case GATEWAY_REQUEST_GUILD_MEMBERS:
        return "GATEWAY_REQUEST_GUILD_MEMBERS";
  case GATEWAY_INVALID_SESSION:
        return "GATEWAY_INVALID_SESSION";
  case GATEWAY_HELLO:
        return "GATEWAY_HELLO";
  case GATEWAY_HEARTBEAT_ACK:
        return "GATEWAY_HEARTBEAT_ACK";
  default:
        DEBUG_PRINT("Invalid gateway opcode:\t%d", opcode);
        abort();
  }
}

static void
_concord_on_gateway_hello(concord_gateway_st *gateway)
{
  unsigned long heartbeat_ms = (unsigned long)jscon_get_integer(jscon_get_branch(gateway->event_data, "heartbeat_interval"));
  DEBUG_ASSERT(heartbeat_ms > 0, "Invalid heartbeat_ms");

  int uvcode = uv_timer_start(&gateway->heartbeat_signal, &_uv_on_heartbeat_signal_cb, 0, heartbeat_ms);
  DEBUG_ASSERT(!uvcode, uv_strerror(uvcode));
}

static void
_concord_gateway_start_identify(concord_gateway_st *gateway)
{
  if (gateway->identify){
    jscon_destroy(gateway->identify);
    gateway->identify = NULL;
  }

  uv_utsname_t buffer;
  int uvcode = uv_os_uname(&buffer);
  DEBUG_ASSERT(!uvcode, "Couldn't fetch system information");


  jscon_list_st *main_list = jscon_list_init();  
  jscon_list_st *helper_list = jscon_list_init();  

  /* https://discord.com/developers/docs/topics/gateway#identify-identify-connection-properties */
  jscon_list_append(helper_list, jscon_string(buffer.sysname, "$os"));
  /* @todo library name should be from a macro */
  jscon_list_append(helper_list, jscon_string("libconcord", "$browser"));
  jscon_list_append(helper_list, jscon_string("libconcord", "$device"));
  jscon_list_append(main_list, jscon_object(helper_list, "properties"));
  /* https://discord.com/developers/docs/topics/gateway#sharding */
  /* @todo */

  /* https://discord.com/developers/docs/topics/gateway#update-status-gateway-status-update-structure */
  jscon_list_append(helper_list, jscon_null("since"));
  jscon_list_append(helper_list, jscon_null("activities"));
  jscon_list_append(helper_list, jscon_string("online","status"));
  jscon_list_append(helper_list, jscon_boolean(false,"afk"));
  jscon_list_append(main_list, jscon_object(helper_list, "presence"));

  /* https://discord.com/developers/docs/topics/gateway#identify-identify-structure */
  jscon_list_append(main_list, jscon_string(gateway->token, "token"));
  jscon_list_append(main_list, jscon_boolean(false, "compress"));
  jscon_list_append(main_list, jscon_integer(50, "large_threshold"));
  jscon_list_append(main_list, jscon_boolean(true, "guild_subscriptions"));
  jscon_list_append(main_list, jscon_integer(GUILD_MESSAGES, "intents"));
  jscon_list_append(main_list, jscon_object(main_list, "d"));
  jscon_list_append(main_list, jscon_integer(GATEWAY_IDENTIFY, "op"));
  
  /* @todo make this a separate function */
  gateway->identify = jscon_object(main_list, NULL);

  char *send_payload = jscon_stringify(gateway->identify, JSCON_ANY);
  DEBUG_PRINT("IDENTIFY PAYLOAD:\n\t%s", send_payload);
  
  bool ret = cws_send_text(gateway->easy_handle, send_payload);
  DEBUG_ASSERT(true == ret, "Couldn't send heartbeat payload");

  safe_free(send_payload);

  jscon_list_destroy(helper_list);
  jscon_list_destroy(main_list);
}

void
Concord_on_text_cb(void *data, CURL *easy_handle, const char *text, size_t len)
{
  concord_gateway_st *gateway = data;

  DEBUG_PRINT("ON_TEXT:\n\t\t%s", text);

  if (gateway->event_data){
    jscon_destroy(gateway->event_data);
    gateway->event_data = NULL;
  }

  jscon_scanf((char*)text, 
              "#t%js " \
              "#s%jd " \
              "#op%jd " \
              "#d%ji",
               gateway->event_name,
               &gateway->seq_number,
               &gateway->opcode,
               &gateway->event_data);

  DEBUG_PRINT("OP:\t\t%d\n\tEVENT_NAME:\t%s\n\tSEQ_NUMBER:\t%d", 
              gateway->opcode, 
              !*gateway->event_name /* "if is empty string" */
                 ?  _concord_gateway_strevent(gateway->opcode)
                 : gateway->event_name, 
              gateway->seq_number);

  switch (gateway->opcode){
  case GATEWAY_DISPATCH:
        break;
  case GATEWAY_HELLO:
        _concord_on_gateway_hello(gateway);
        _concord_gateway_start_identify(gateway);
        break;
  case GATEWAY_HEARTBEAT_ACK:
        break; 
  default:
        DEBUG_PRINT("Not yet implemented Gateway Opcode: %d", gateway->opcode);
        abort();
  }

  (void)len;
  (void)easy_handle;
}

void
Concord_on_close_cb(void *data, CURL *easy_handle, enum cws_close_reason cwscode, const char *reason, size_t len)
{
  DEBUG_PRINT("CLOSE=%4d %zd bytes '%s'", cwscode, len, reason);

  (void)easy_handle;
  (void)data;
}

static void
_uv_disconnect_cb(uv_timer_t *req)
{
  DEBUG_PUTS("ATTEMPTING TO DISCONNECT FROM GATEWAY ...");
  concord_gateway_st *gateway = uv_handle_get_data((uv_handle_t*)req);

  char reason[] = "Disconnecting!";
  bool ret = cws_close(gateway->easy_handle, CWS_CLOSE_REASON_NORMAL, reason, strlen(reason));
  DEBUG_ASSERT(true == ret, "Couldn't disconnect from gateway gracefully");

  _uv_perform_cb(&gateway->context->poll_handle, uv_is_closing((uv_handle_t*)&gateway->context->poll_handle), UV_READABLE|UV_DISCONNECT);

  uv_timer_stop(&gateway->timeout);
  uv_close((uv_handle_t*)&gateway->timeout, NULL);
}

static void
_uv_on_force_close_cb(uv_async_t *req)
{
  concord_gateway_st *gateway = uv_handle_get_data((uv_handle_t*)req);

  int uvcode = uv_timer_start(&gateway->timeout, &_uv_disconnect_cb, 0, 0);
  DEBUG_ASSERT(!uvcode, uv_strerror(uvcode));

  uv_close((uv_handle_t*)req, NULL);

  uv_timer_stop(&gateway->heartbeat_signal);
  uv_close((uv_handle_t*)&gateway->heartbeat_signal, NULL);
}

static void
_concord_gateway_run(void *ptr)
{
  concord_gateway_st *gateway = ptr; 

  uv_async_init(gateway->loop, &gateway->async, &_uv_on_force_close_cb);
  uv_handle_set_data((uv_handle_t*)&gateway->async, gateway);

  curl_multi_add_handle(gateway->multi_handle, gateway->easy_handle);

  uv_run(gateway->loop, UV_RUN_DEFAULT);

  curl_multi_remove_handle(gateway->multi_handle, gateway->easy_handle);
}

void
concord_gateway_connect(concord_st *concord)
{
  concord_gateway_st *gateway = concord->gateway;

  if (RUNNING == gateway->status){
    DEBUG_PUTS("Gateway already running, returning"); 
    return;
  }

  gateway->status = RUNNING; /* not really running just yet, trying to connect ... */

  uv_thread_create(&gateway->thread_id, &_concord_gateway_run, gateway);
}

void
concord_gateway_disconnect(concord_st *concord)
{
  if (INNACTIVE == concord->gateway->status){
    DEBUG_PUTS("Gateway already innactive, returning"); 
    return;
  }

  _concord_gateway_disconnect(concord->gateway);
}

int
concord_gateway_isrunning(concord_st *concord)
{
  return (RUNNING == concord->gateway->status);
}
