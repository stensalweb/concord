# Concord TODO

This document describes features to be incorporated in the future.

## HIGH

- Find out why sometimes calling `jscon_scanf()` at `_concord_ld_guild()` leads to a segfault
- [Check rate-limits dynamically](https://discord.com/developers/docs/topics/rate-limits#rate-limits)
- Hashtable size should be dynamically increased as necessary
- Create objects (channel, user, etc) print functions for easier debugging

## MEDIUM

- There are more advantageous options for doing async transfers, with either [poll or epoll](https://daniel.haxx.se/docs/poll-vs-select.html). I must learn about each of those options, check whether those advantages apply to this library's goal or not. ( see `concord_dispatch()` )

## LOW

- (?) Implement a prediction system for auto-performing async requests which hold a dependency
- Custom hashtable implementation that share entries among them (for when hashtables hold the same entries but with different keys)

  

