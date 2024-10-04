all: build

prepare-c:
	set -xe
	CFLAGS="-Wall -Wextra -g -O3"

build-server: prepare-c
	cc kvcached.c -o build/kvcached

build-client: prepare-c
	cc kvcachecmd.c -o build/kvcachemd

build-controller: prepare-c
	cc kvcachectl.c -o build/kvcachectl

build: build-server build-client build-controller

install: build
	install -m 555 build/kvcached /usr/local/bin
	install -m 555 build/kvcachecmd /usr/local/bin
	install -m 555 build/kvcachectl /usr/local/bin

deinstall:
	rm /usr/local/bin/kvcached
	rm /usr/local/bin/kvcachecmd
	rm /usr/local/bin/kvcachectl

clean:
	rm build/kvcachecmd
	rm build/kvcached
	rm build/kvcachectl
