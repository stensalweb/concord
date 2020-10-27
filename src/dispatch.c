#include <stdio.h>
#include <stdlib.h>

#include <libconcord.h>

#include "hashtable.h"
#include "debug.h"
#include "ratelimit.h"

static void
_curl_check_multi_info(concord_utils_st *utils)
{
  /* See how the transfers went */
  CURLMsg *msg; /* for picking up messages with the transfer status */
  int pending; /*how many messages are left */
  long http_code; /* http response code */
  while ((msg = curl_multi_info_read(utils->multi_handle, &pending)))
  {
    if (CURLMSG_DONE != msg->msg)
      continue;

    /* Find out which handle this message is about */
    char easy_key[18];
    sprintf(easy_key, "%p", msg->easy_handle);
    struct concord_conn_s *conn = dictionary_get(utils->easy_dict, easy_key);

    curl_easy_getinfo(conn->easy_handle, CURLINFO_RESPONSE_CODE, &http_code);
    debug_assert(429 != http_code, "Being ratelimited");

    /* execute load callback to perform change in object */
    if (NULL != conn->response_body.str){
      //debug_puts(conn->response_body.str);
      (*conn->load_cb)(conn->p_object, &conn->response_body);

      conn->p_object = NULL;

      safe_free(conn->response_body.str);
      conn->response_body.size = 0;
    }

    curl_multi_remove_handle(utils->multi_handle, conn->easy_handle);

    int remaining = dictionary_get_strtoll(utils->header, "x-ratelimit-remaining");
    debug_print("Remaining connections: %d", remaining);
    if (0 == remaining){
      long long delay_ms = Concord_parse_ratelimit_header(utils->header, true);
      debug_print("Delay_ms: %lld", delay_ms);
      /* @todo sleep is blocking, we don't want this */
      uv_sleep(delay_ms);
    }

    do {
      Concord_queue_pop(utils, conn->bucket);
    } while (remaining--);
  }
}

/* wrapper around curl_multi_perform() , using poll() */
void
concord_dispatch(concord_st *concord)
{
  concord_utils_st *utils = &concord->utils;

  Concord_start_client_buckets(utils);

  int transfers_running = 0, tmp = 0; /* keep number of running handles */
  int repeats = 0;
  do {
    CURLMcode mcode;
    int numfds;

    mcode = curl_multi_perform(utils->multi_handle, &transfers_running);
    if (CURLM_OK == mcode){
      /* wait for activity, timeout or "nothing" */
      mcode = curl_multi_wait(utils->multi_handle, NULL, 0, 500, &numfds);
    }
    debug_assert(CURLM_OK == mcode, curl_easy_strerror(mcode));

    if (tmp != transfers_running){
      debug_print("Transfers Running: %d\n\tTransfers On Hold: %ld", transfers_running, utils->transfers_onhold);
      _curl_check_multi_info(utils);
      tmp = transfers_running;
    }

    /* numfds being zero means either a timeout or no file descriptor to
        wait for. Try timeout on first occurrences, then assume no file
        descriptors and no file descriptors to wait for mean wait for
        100 milliseconds. */
    if (0 == numfds){
      ++repeats; /* count number of repeated zero numfds */
      if (repeats > 1){
        uv_sleep(100); /* sleep 100 milliseconds */
      }
    } else {
      repeats = 0;
    }
  } while (utils->transfers_onhold || transfers_running);
  debug_assert(0 == utils->transfers_onhold, "There are still transfers on hold");

  Concord_reset_client_buckets(utils);
}

