# Concord TODO

This document describes features to be incorporated in the future.

## HIGH

- Implement Discord Gateway (WebSockets) support
  - Intuitive event handler
  - Resume logic on error
- [Check rate-limits dynamically](https://discord.com/developers/docs/topics/rate-limits#rate-limits)
- Hashtable size should be dynamically increased as necessary
- Token should go through a hashing and dehashing function

## MEDIUM

- Add a folder inside src for third party header files
- Custom hashtable implementation that share entries among them (for when hashtables hold the same entries but with different keys)

## LOW

- Apply a modifiable limit on how many easy handles are created while doing `ASYNC_IO` requests, before it dispatches automatically, doing this saves a lot of memory
- `curl_update` needs to update nghttp path

  

