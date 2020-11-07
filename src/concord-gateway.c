#include <stdio.h>
#include <stdlib.h>

#include <libconcord.h>
#include "concord-common.h"

#include "hashtable.h"
#include "debug.h"


concord_gateway_st*
Concord_gateway_init()
{
  concord_gateway_st *new_gateway = safe_malloc(sizeof *new_gateway);
  
  new_gateway->loop = safe_malloc(sizeof *new_gateway->loop);
  uv_loop_init(new_gateway->loop);
  uv_loop_set_data(new_gateway->loop, new_gateway);

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

void
Concord_gateway_destroy(concord_gateway_st *gateway)
{
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

    cws_close(gateway->easy_handle, CWS_CLOSE_REASON_NORMAL, "finished", SIZE_MAX);
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
  CURLMcode mcode;
  int uvcode;
  int events = 0;


  switch (action){
  case CURL_POLL_IN:
  case CURL_POLL_OUT:
  case CURL_POLL_INOUT:
      if (p_socket){
        gateway->context = (struct concord_context_s*)p_socket;
      } else {
        gateway->context = Concord_context_init(gateway->loop, sockfd);
      }

      mcode = curl_multi_assign(gateway->multi_handle, sockfd, gateway->context);
      DEBUG_ASSERT(CURLM_OK == mcode, curl_multi_strerror(mcode));

      if (action != CURL_POLL_IN) events |= UV_WRITABLE;
      if (action != CURL_POLL_OUT) events |= UV_READABLE;

      uvcode = uv_poll_start(&gateway->context->poll_handle, events, &_uv_perform_cb);
      DEBUG_ASSERT(!uvcode, uv_strerror(uvcode));
      break;
  case CURL_POLL_REMOVE:
      if (p_socket){
        uvcode = uv_poll_stop(&((struct concord_context_s*)p_socket)->poll_handle);
        DEBUG_ASSERT(!uvcode, uv_strerror(uvcode));

        Concord_context_destroy((struct concord_context_s*)p_socket);

        mcode = curl_multi_assign(gateway->multi_handle, sockfd, NULL);
        DEBUG_ASSERT(CURLM_OK == mcode, curl_multi_strerror(mcode));
      }
      break;
  default:
      DEBUG_PRINT("Unknown CURL_POLL_XXX option encountered\n\tCode: %d", action);
      abort();
  }

  (void)easy_handle;

  return 0;
}

void
Concord_on_connect_cb(void *data, CURL *easy_handle, const char *ws_protocols)
{
  concord_gateway_st *gateway = data;

  gateway->status = RUNNING;

  DEBUG_PRINT("Connected, WS-Protocols: '%s'", ws_protocols);

  (void)easy_handle;
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
}

static void
_uv_on_heartbeat_signal_cb(uv_timer_t *req)
{
  concord_gateway_st *gateway = uv_handle_get_data((uv_handle_t*)req);

  DEBUG_PRINT("REPEAT_MS: %ld", uv_timer_get_repeat(&gateway->heartbeat_signal));

  _concord_heartbeat_send(gateway);

  CURLMcode mcode = curl_multi_socket_action(gateway->multi_handle, gateway->context->sockfd, CURL_CSELECT_IN, &gateway->transfers_running);
  DEBUG_ASSERT(CURLM_OK == mcode, curl_multi_strerror(mcode));
}

void
Concord_on_text_cb(void *data, CURL *easy_handle, const char *text, size_t len)
{
  concord_gateway_st *gateway = data;

  DEBUG_PRINT("ON_TEXT:\n\t\t%s", text);

  if (gateway->event_data){
    jscon_destroy(gateway->event_data);
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
              gateway->event_name, 
              gateway->seq_number);

  int uvcode;
  switch (gateway->opcode){
  case GATEWAY_HELLO:
   {
        ulong heartbeat_ms = (ulong)jscon_get_integer(jscon_get_branch(gateway->event_data, "heartbeat_interval"));
        DEBUG_ASSERT(heartbeat_ms > 0, "Invalid heartbeat_ms");

        /* @todo figure out why timer is not accurate */
        uvcode = uv_timer_start(&gateway->heartbeat_signal, &_uv_on_heartbeat_signal_cb, 0, heartbeat_ms);
        DEBUG_ASSERT(!uvcode, uv_strerror(uvcode));
        break;
   }
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
  concord_gateway_st *gateway = data;
  DEBUG_PRINT("CLOSE=%4d %zd bytes '%s'", cwscode, len, reason);

  gateway->status = INNACTIVE;
  uv_stop(gateway->loop);

  (void)easy_handle;
}

void
Concord_gateway_run(concord_gateway_st *gateway)
{
  if (RUNNING == gateway->status){
    DEBUG_PUTS("Gateway already running, returning"); 
    return;
  }
  curl_multi_add_handle(gateway->multi_handle, gateway->easy_handle);

  int uvcode = uv_run(gateway->loop, UV_RUN_DEFAULT);
  DEBUG_ASSERT(!uvcode, uv_strerror(uvcode));

  curl_multi_remove_handle(gateway->multi_handle, gateway->easy_handle);
}
