# Concord TODO

This document describes features to be incorporated in the future.

## HIGH

- Deal with the ratelimiting issue by grouping easy handles of the same type in a particular FIFO (named pipe) architecture. There can be multiple name pipes being scanned at once, the event loop will keep checking each of them until every one is depleted. What named pipe is popped at which order doesn't matter, as each will have its own ratelimiting rules to take care of. A named pipe will group `easy_handles` with common endpoint, and/or major parameters. Then, as the response header is being parsed it will check for the 'per route' bucket hashes returned by the Discord API.
- Replace `curl_multi_wait` with its [event driven](https://ec.haxx.se/libcurl/libcurl-drive/libcurl-drive-multi-socket) counterpart
  - Alongside with libuv
- Implement Discord Gateway (WebSockets) support
  - Intuitive event handler
  - Resume logic on error
- [Check rate-limits dynamically](https://discord.com/developers/docs/topics/rate-limits#rate-limits)
  - Use 'current timestamp' instead of 'reset after'
  - Deal with 429 condition
  - Create parsing function to extract X-RateLimit information
  - Create a exception function that detects when user is being rate limited, and act accordingly
- Hashtable size should be dynamically increased as necessary
- Create objects (channel, user, etc) print functions for easier debugging
- Token should go through a hashing and dehashing function

## MEDIUM

- Custom hashtable implementation that share entries among them (for when hashtables hold the same entries but with different keys)

## LOW

- Apply a modifiable limit on how many easy handles are created while doing `ASYNC_IO` requests, before it dispatches automatically, doing this saves a lot of memory
- `curl_update` needs to update nghttp path

  

