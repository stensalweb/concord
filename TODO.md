# Concord TODO

This document describes features to be incorporated in the future.

## HIGH

- SCHEDULE is not working as it should, it should create `easy_handles` dynamically as necessity arises, and execute them before hitting a limit.
- [Check rate-limits dynamically](https://discord.com/developers/docs/topics/rate-limits#rate-limits)
- Hashtable size should be dynamically increased as necessary
- Create objects (channel, user, etc) print functions for easier debugging
- Token should go through a hashing and dehashing function

## MEDIUM

- Use `curl_share` when initing sync easy handles
- There are more advantageous options for doing async transfers, with either [poll or epoll](https://daniel.haxx.se/docs/poll-vs-select.html). I must learn about each of those options, check whether those advantages apply to this library's goal or not. ( see `concord_dispatch()` )
  - Update libcurl to at least 7.66.0 for [`curl_multi_poll()`](https://daniel.haxx.se/docs/poll-vs-select.html)

## LOW

- `curl_update` needs to update nghttp path
- (?) Implement a prediction system for auto-performing async requests which hold a dependency
- Custom hashtable implementation that share entries among them (for when hashtables hold the same entries but with different keys)

  

