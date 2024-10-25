all: build

CFLAGS = -Wall -Wextra -ggdb -O3 -Wno-pointer-sign -isystem third_party
LIBEV = third_party/libev/ev.o
CC = gcc

all:
	set -xe
	mkdir -p build

configure:
	git submodule update --init --recursive
	./third_party/libev/configure --enable-static

compile-libev:
	cd third_party/libev/ && make

build-server: compile-libev
	$(CC) $(CFLAGS) kvcached.c -o build/kvcached $(LIBEV)

build-client: compile-libev
	$(CC) $(CFLAGS) kvcachecmd.c -o build/kvcachecmd $(LIBEV)

build-controller:
	$(CC) $(CFLAGS) kvcachectl.c -o build/kvcachectl

build: build-server build-client

install: build
	install -m 555 build/kvcached /usr/local/bin
	install -m 555 build/kvcachecmd /usr/local/bin
	install -m 555 build/kvcachectl /usr/local/bin

deinstall:
	rm /usr/local/bin/kvcached
	rm /usr/local/bin/kvcachecmd
	rm /usr/local/bin/kvcachectl

clean:
	rm -r build
