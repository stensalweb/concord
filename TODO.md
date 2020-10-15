# Concord TODO

This document describes features to be incorporated in the future.

## HIGH

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

- There are more advantageous options for doing async transfers, with either [poll or epoll](https://daniel.haxx.se/docs/poll-vs-select.html). I must learn about each of those options, check whether those advantages apply to this library's goal or not. ( see `concord_dispatch()` )
  - Update libcurl to at least 7.66.0 for [`curl_multi_poll()`](https://daniel.haxx.se/docs/poll-vs-select.html)
- Custom hashtable implementation that share entries among them (for when hashtables hold the same entries but with different keys)

## LOW

- (?) rename SYNC and SCHEDULE references to BLOCKING and MULTIPLEX, respectively. Though I'm not sure which meanings are more intuitive.
- `curl_update` needs to update nghttp path

  

