#include <stdio.h>
#include <stdlib.h>

#include <libconcord.h>
#include "concord-common.h"

#include "hashtable.h"
#include "debug.h"


concord_ws_t*
Concord_ws_init(char token[])
{
  concord_ws_t *new_ws = safe_calloc(1, sizeof *new_ws);
  
  new_ws->loop = safe_malloc(sizeof *new_ws->loop);
  uv_loop_init(new_ws->loop);
  uv_loop_set_data(new_ws->loop, new_ws);

  new_ws->token = strndup(token, strlen(token)-1);
  DEBUG_ASSERT(NULL != new_ws->token, "Out of memory");

  uv_timer_init(new_ws->loop, &new_ws->timeout);
  uv_handle_set_data((uv_handle_t*)&new_ws->timeout, new_ws);

  uv_timer_init(new_ws->loop, &new_ws->heartbeat_timer);
  uv_handle_set_data((uv_handle_t*)&new_ws->heartbeat_timer, new_ws);

  new_ws->easy_handle = Concord_ws_easy_init(new_ws);
  new_ws->multi_handle = Concord_ws_multi_init(new_ws);

  return new_ws;
}

static void
_uv_on_walk_cb(uv_handle_t *handle, void *arg)
{
  uv_close(handle, NULL);

  (void)arg;
}

static void
_concord_ws_disconnect(concord_ws_t *ws)
{
  uv_async_send(&ws->async);
  uv_thread_join(&ws->thread_id);

  DEBUG_ASSERT(DISCONNECTED == ws->status, "Couldn't disconnect from gateway");
}

void
Concord_ws_destroy(concord_ws_t *ws)
{
  if ((CONNECTING|CONNECTED) & ws->status){
    _concord_ws_disconnect(ws);
  }

  if (ws->context){
    Concord_context_destroy(ws->context);
    ws->context = NULL;
  }

  curl_multi_cleanup(ws->multi_handle);

  cws_free(ws->easy_handle);

  int uvcode = uv_loop_close(ws->loop);
  if (UV_EBUSY == uvcode){ //there are still handles that need to be closed
    uv_walk(ws->loop, &_uv_on_walk_cb, NULL); //close each handle encountered

    uvcode = uv_run(ws->loop, UV_RUN_DEFAULT); //run the loop again to close remaining handles
    DEBUG_ASSERT(!uvcode, uv_strerror(uvcode));

    uvcode = uv_loop_close(ws->loop); //finally, close the loop
    DEBUG_ASSERT(!uvcode, uv_strerror(uvcode));
  }
  safe_free(ws->loop);

  if (ws->payload.event_data){
    jscon_destroy(ws->payload.event_data);
    ws->payload.event_data = NULL;
  }

  safe_free(ws->token);

  safe_free(ws); 
}

static void
_uv_perform_cb(uv_poll_t *req, int uvstatus, int events)
{
  DEBUG_ASSERT(!uvstatus, uv_strerror(uvstatus));

  concord_ws_t *ws = uv_handle_get_data((uv_handle_t*)req);

  int flags = 0;
  if (events & UV_READABLE) flags |= CURL_CSELECT_IN;
  if (events & UV_WRITABLE) flags |= CURL_CSELECT_OUT;

  CURLMcode mcode = curl_multi_socket_action(ws->multi_handle, ws->context->sockfd, flags, &ws->transfers_running);
  DEBUG_ASSERT(CURLM_OK == mcode, curl_multi_strerror(mcode));


  CURLMsg *msg; /* for picking up messages with the transfer status */
  int pending; /*how many messages are left */

  while ((msg = curl_multi_info_read(ws->multi_handle, &pending)))
  {
    if (CURLMSG_DONE != msg->msg)
      continue;
    
    DEBUG_PRINT("HTTP completed with status %d '%s'", msg->data.result, curl_easy_strerror(msg->data.result));

    ws->status = DISCONNECTED;
    uv_stop(ws->loop);
  }
}

static void
_uv_on_timeout_cb(uv_timer_t *req)
{
  concord_ws_t *ws = uv_handle_get_data((uv_handle_t*)req);

  CURLMcode mcode = curl_multi_socket_action(ws->multi_handle, CURL_SOCKET_TIMEOUT, 0, &ws->transfers_running);
  DEBUG_ASSERT(CURLM_OK == mcode, curl_multi_strerror(mcode));
}

int
Concord_ws_timeout_cb(CURLM *multi_handle, long timeout_ms, void *p_userdata)
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
Concord_ws_socket_cb(CURL *easy_handle, curl_socket_t sockfd, int action, void *p_userdata, void *p_socket)
{
  concord_ws_t *ws = p_userdata;
  int uvcode;
  int events = 0;


  switch (action){
  case CURL_POLL_IN:
  case CURL_POLL_OUT:
  case CURL_POLL_INOUT:
      if (!ws->context){
        ws->context = Concord_context_init(ws->loop, sockfd);
      }

      if (action != CURL_POLL_IN) events |= UV_WRITABLE;
      if (action != CURL_POLL_OUT) events |= UV_READABLE;

      uvcode = uv_poll_start(&ws->context->poll_handle, events, &_uv_perform_cb);
      DEBUG_ASSERT(!uvcode, uv_strerror(uvcode));
      break;
  case CURL_POLL_REMOVE:
      if (ws->context){
        uvcode = uv_poll_stop(&ws->context->poll_handle);
        DEBUG_ASSERT(!uvcode, uv_strerror(uvcode));

        Concord_context_destroy(ws->context);
        ws->context = NULL;
      }
      break;
  default:
      DEBUG_ERR("Unknown CURL_POLL_XXXX option encountered\n\tCode: %d", action);
  }

  (void)easy_handle;
  (void)p_socket;

  return 0;
}

void
Concord_on_connect_cb(void *data, CURL *easy_handle, const char *ws_protocols)
{
  concord_ws_t *ws = data;

  DEBUG_PRINT("Connected, WS-Protocols: '%s'", ws_protocols);

  ws->status = CONNECTED;

  (void)easy_handle;
  (void)ws_protocols;
}

static void
_uv_on_heartbeat_cb(uv_timer_t *req)
{
  concord_ws_t *ws = uv_handle_get_data((uv_handle_t*)req);

  DEBUG_PRINT("REPEAT_MS: %ld", uv_timer_get_repeat(&ws->heartbeat_timer));

  char send_payload[250];

  if (0 == ws->payload.seq_number){
    snprintf(send_payload, 249, "{\"op\": 1, \"d\": null}");
  } else {
    snprintf(send_payload, 249, "{\"op\": 1, \"d\": %d}", ws->payload.seq_number);
  }

  DEBUG_NOTOP_PRINT("HEARTBEAT_PAYLOAD:\n\t\t%s", send_payload);
  bool ret = cws_send_text(ws->easy_handle, send_payload);
  DEBUG_ASSERT(true == ret, "Couldn't send heartbeat payload");

  _uv_perform_cb(&ws->context->poll_handle, uv_is_closing((uv_handle_t*)&ws->context->poll_handle), UV_READABLE);
}

char*
_concord_payload_strevent(enum ws_opcode opcode)
{

/* if case matches, return token as string */
#define CASE_RETURN_STR(opcode) case opcode: return #opcode

  switch(opcode){
  CASE_RETURN_STR(GATEWAY_DISPATCH);
  CASE_RETURN_STR(GATEWAY_HEARTBEAT);
  CASE_RETURN_STR(GATEWAY_IDENTIFY);
  CASE_RETURN_STR(GATEWAY_PRESENCE_UPDATE);
  CASE_RETURN_STR(GATEWAY_VOICE_STATE_UPDATE);
  CASE_RETURN_STR(GATEWAY_RESUME);
  CASE_RETURN_STR(GATEWAY_RECONNECT);
  CASE_RETURN_STR(GATEWAY_REQUEST_GUILD_MEMBERS);
  CASE_RETURN_STR(GATEWAY_INVALID_SESSION);
  CASE_RETURN_STR(GATEWAY_HELLO);
  CASE_RETURN_STR(GATEWAY_HEARTBEAT_ACK);

  default:
  DEBUG_ERR("Invalid ws opcode:\t%d", opcode);
  }
}

static void
_concord_on_ws_hello(concord_ws_t *ws)
{
  unsigned long heartbeat_ms = (unsigned long)jscon_get_integer(jscon_get_branch(ws->payload.event_data, "heartbeat_interval"));
  DEBUG_ASSERT(heartbeat_ms > 0, "Invalid heartbeat_ms");

  int uvcode = uv_timer_start(&ws->heartbeat_timer, &_uv_on_heartbeat_cb, 0, heartbeat_ms);
  DEBUG_ASSERT(!uvcode, uv_strerror(uvcode));
}

static void
_concord_ws_start_identify(concord_ws_t *ws)
{
  if (NULL == ws->identify){
    uv_utsname_t buffer;
    int uvcode = uv_os_uname(&buffer);
    DEBUG_ASSERT(!uvcode, "Couldn't fetch system information");

    /* https://discord.com/developers/docs/topics/gateway#identify-identify-connection-properties */
    jscon_item_t *properties = jscon_object("properties");
    jscon_append(properties, jscon_string("$os", buffer.sysname));
    jscon_append(properties, jscon_string("$browser", "libconcord"));
    jscon_append(properties, jscon_string("$device", "libconcord"));

    /* https://discord.com/developers/docs/topics/gateway#sharding */
    /* @todo */

    /* https://discord.com/developers/docs/topics/gateway#update-status-gateway-status-update-structure */
    jscon_item_t *presence = jscon_object("presence");
    jscon_append(presence, jscon_null("since"));
    jscon_append(presence, jscon_null("activities"));
    jscon_append(presence, jscon_string("status", "online"));
    jscon_append(presence, jscon_boolean("afk", false));

    /* https://discord.com/developers/docs/topics/gateway#identify-identify-structure */
    jscon_item_t *event_data = jscon_object("d");
    jscon_append(event_data, jscon_string("token", ws->token));
    jscon_append(event_data, jscon_integer("intents", GUILD_MESSAGES));
    jscon_append(event_data, properties);
    jscon_append(event_data, presence);

    ws->identify = jscon_object(NULL);
    jscon_append(ws->identify, jscon_integer("op", GATEWAY_IDENTIFY));
    jscon_append(ws->identify, event_data);
  }

  char *send_payload = jscon_stringify(ws->identify, JSCON_ANY);
  DEBUG_PRINT("IDENTIFY PAYLOAD:\n\t%s", send_payload);
  
  bool ret = cws_send_text(ws->easy_handle, send_payload);
  DEBUG_ASSERT(true == ret, "Couldn't send heartbeat payload");

  safe_free(send_payload);
}

void
Concord_on_text_cb(void *data, CURL *easy_handle, const char *text, size_t len)
{
  concord_ws_t *ws = data;

  DEBUG_PRINT("ON_TEXT:\n\t\t%s", text);

  if (ws->payload.event_data){
    jscon_destroy(ws->payload.event_data);
    ws->payload.event_data = NULL;
  }

  jscon_scanf((char*)text, 
              "%s[t]" \
              "%d[s]" \
              "%d[op]" \
              "%ji[d]",
               ws->payload.event_name,
               &ws->payload.seq_number,
               &ws->payload.opcode,
               &ws->payload.event_data);

  DEBUG_NOTOP_PRINT("OP:\t\t%s\n\tEVENT_NAME:\t%s\n\tSEQ_NUMBER:\t%d", 
              _concord_payload_strevent(ws->payload.opcode), 
              !*ws->payload.event_name      /* if event name exists */
                 ? "NULL"                   /* print NULL */
                 : ws->payload.event_name,  /* otherwise, event name */
              ws->payload.seq_number);

  switch (ws->payload.opcode){
  case GATEWAY_DISPATCH:
        break;
  case GATEWAY_HELLO:
        _concord_on_ws_hello(ws);
        _concord_ws_start_identify(ws);
        break;
  case GATEWAY_HEARTBEAT_ACK:
        break; 
  default:
        DEBUG_ERR("Not yet implemented Gateway Opcode: %d", ws->payload.opcode);
  }

  (void)len;
  (void)easy_handle;
}

void
Concord_on_close_cb(void *data, CURL *easy_handle, enum cws_close_reason cwscode, const char *reason, size_t len)
{
  concord_ws_t *ws = data;

  DEBUG_PRINT("CLOSE=%4d %zd bytes '%s'", cwscode, len, reason);

  ws->status = DISCONNECTING;

  (void)easy_handle;
  (void)cwscode;
  (void)len;
  (void)reason;
}

static void
_uv_disconnect_cb(uv_timer_t *req)
{
  concord_ws_t *ws = uv_handle_get_data((uv_handle_t*)req);

  char reason[] = "Disconnecting!";
  bool ret = cws_close(ws->easy_handle, CWS_CLOSE_REASON_NORMAL, reason, strlen(reason));
  DEBUG_ONLY_ASSERT(true == ret, "Couldn't disconnect gracefully from WebSockets");

  uv_timer_stop(&ws->timeout);
  uv_close((uv_handle_t*)&ws->timeout, NULL);

  _uv_perform_cb(&ws->context->poll_handle, uv_is_closing((uv_handle_t*)&ws->context->poll_handle), UV_READABLE|UV_DISCONNECT);

  DEBUG_NOTOP_PUTS("Disconnected");
}

static void
_uv_on_force_close_cb(uv_async_t *req)
{
  concord_ws_t *ws = uv_handle_get_data((uv_handle_t*)req);

  DEBUG_PUTS("Attempting to disconnect from gateway ...");
  int uvcode = uv_timer_start(&ws->timeout, &_uv_disconnect_cb, 0, 0);
  DEBUG_ASSERT(!uvcode, uv_strerror(uvcode));

  uv_close((uv_handle_t*)req, NULL);

  uv_timer_stop(&ws->heartbeat_timer);
  uv_close((uv_handle_t*)&ws->heartbeat_timer, NULL);
}

static void
_concord_ws_run(void *ptr)
{
  concord_ws_t *ws = ptr; 

  uv_async_init(ws->loop, &ws->async, &_uv_on_force_close_cb);
  uv_handle_set_data((uv_handle_t*)&ws->async, ws);

  curl_multi_add_handle(ws->multi_handle, ws->easy_handle);

  uv_run(ws->loop, UV_RUN_DEFAULT);

  curl_multi_remove_handle(ws->multi_handle, ws->easy_handle);
}

void
concord_ws_connect(concord_t *concord)
{
  if ((CONNECTING|CONNECTED) & concord->ws->status){
    DEBUG_NOTOP_PUTS("Gateway already connected, returning ..."); 
    return;
  }

  concord->ws->status = CONNECTING;

  uv_thread_create(&concord->ws->thread_id, &_concord_ws_run, concord->ws);
}

void
concord_ws_disconnect(concord_t *concord)
{
  if ((DISCONNECTING|DISCONNECTED) & concord->ws->status){
    DEBUG_NOTOP_PUTS("Gateway already disconnected, returning ..."); 
    return;
  }

  concord->ws->status = DISCONNECTING;

  _concord_ws_disconnect(concord->ws);
}

int
concord_ws_isrunning(concord_t *concord)
{
  return ((CONNECTING|CONNECTED) & concord->ws->status);
}
