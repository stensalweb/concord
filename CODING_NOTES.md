# Concord Coding Notes

This document describes the formatting rules followed by this library.

## Functions

Public functions are prefixed by a [`concord_`] tag. Private functions with local scopes (static functions) are prefixed by a [`_concord_`] tag, and private functions which are made accessible by private headers, are prefixed by a capitalized [`Concord_`] tag. Functions that are redefined by macros, are prefixed by double underscores [`__concord_`].
