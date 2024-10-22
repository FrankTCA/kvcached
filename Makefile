all: build

CFLAGS = -Wall -Wextra -ggdb -O3 -Wno-pointer-sign
LIBEV = third_party/libev/ev.o

prepare-c:
	set -xe
	mkdir -p build

compile-libev:
	./compile-libev.sh

build-server: prepare-c compile-libev
	cc $(CFLAGS) kvcached.c -o build/kvcached $(LIBEV)

build-client: prepare-c compile-libev
	cc $(CFLAGS) kvcachecmd.c -o build/kvcachecmd $(LIBEV)

build-controller: prepare-c
	cc $(CFLAGS) kvcachectl.c -o build/kvcachectl

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
