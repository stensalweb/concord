# Concord

This document describes the formatting rules followed by this library.

## General Naming Rules

[C Coding Style](https://github.com/charlie-wong/coding-style/blob/master/c/Naming.rst), by [charlie-wong](https://github.com/charlie-wong)

## Functions

Public functions are prefixed by a [`concord_`] tag. Private functions with local scopes (static functions) are prefixed by a [`_concord_`] tag, and private functions which are made accessible by specific headers are prefixed by a [`Concord_`] tag.
  
