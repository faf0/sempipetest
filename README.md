# Overview

The program `sempipetest` starts a user-defined
number of producer and consumer threads.
The producers produces records of a fixed size
and write them into a pipe.
The consumers read the items from the pipe
and print their content.

# Installation

The program can be compiled by running `make` inside the folder
where `sempipetest.c` and `Makefile` are located.

# Usage

The general usage syntax is:
    sempipetest [-p <numproducers>] [-c <numconsumers>] [-i <itemsperproducer>] [-m <maxinpipe>]
where
* `numproducers` is the number of producer threads.
  If this parameter is unspecified, `4` will be used by default.
* `numconsumers` is the number of consumer threads.
  If this parameter is unspecified, `4` will be used by default.
* `itemsperproducer` is the number of items that each producer creates.
  If this parameter is unspecified, `4` will be used by default.
* `maxinpipe` denotes the maximum number of items which are in a
  pipe at a time.
  If this parameter is unspecified, `4` will be used by default.

