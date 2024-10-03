#!/bin/sh

kvcache-client "$@" | nc -p 6969 -w 5 localhost 8008
