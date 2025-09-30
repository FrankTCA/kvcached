#!/bin/sh

set -xe

rm -rf build
mkdir build

CFLAGS="-Wall -Wextra -ggdb -O0 -std=c11"
ACO="./third_party/libaco/"
ACO_FLAGS="build/aco.o build/acosw.o"

cc -O3 -c $ACO/aco.c -o build/aco.o
cc -O3 -c $ACO/acosw.S -o build/acosw.o

cc $CFLAGS $ACO_FLAGS -Ithird_party/ d.c -o build/kvcached
cc $CFLAGS cmd.c -o build/kvcachecmd
