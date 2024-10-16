all: build

CFLAGS = -Wall -Wextra -ggdb -O3 -Wno-pointer-sign

prepare-c:
	set -xe
	mkdir -p build

build-server: prepare-c
	cc $(CFLAGS) kvcached.c -o build/kvcached -lev

build-client: prepare-c
	cc $(CFLAGS) kvcachecmd.c -o build/kvcachecmd

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
