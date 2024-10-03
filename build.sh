#!/bin/sh

set -xe

CFLAGS="-Wall -Wextra -g -O3"

cc kvcached.c -o kvcached $CFLAGS
cc client.c -o client $CFLAGS
