# Concord TODO

This document describes features to be incorporated in the future.

## HIGH

- [Check rate-limits dynamically](https://discord.com/developers/docs/topics/rate-limits#rate-limits)
- Hashtable size should be dynamically increased as necessary
- Some operations shouldn't create individual hashtable entries based on the endpoint, for examples, `concord_get_channel_messages()` shouldn't create a individual `easy_handle` for each message it fetches, instead it should reuse the same `easy_handle` between every call
- Create objects (channel, user, etc) print functions for easier debugging

## MEDIUM

- Create endpoint format macros, that will also beused as the hashtable entry's key

## LOW

- (?) Implement a prediction system for auto-performing async requests which hold a dependency

  

