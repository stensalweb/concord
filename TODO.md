# Concord TODO

This document describes features to be incorporated in the future.

## HIGH

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

- `curl_update` needs to update nghttp path

  

