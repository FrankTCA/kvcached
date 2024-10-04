all: build

prepare-c:
	set -xe
	CFLAGS="-Wall -Wextra -g -O3"

build-server: prepare-c
	echo "==> Building server..."
	cc kvcached.c -o build/kvcached

build-client: prepare-c
	echo "==> Building client..."
	cc client.c -o build/kvcache-client

build-controller: prepare-c
	echo "==> Building controller..."
	cc kvcachectl.c -o build/kvcachectl

build: build-server build-client build-controller
	echo "==> All modules built"

install: build
	install -m 555 build/kvcached /usr/local/bin
	install -m 555 build/kvcache-client /usr/local/bin
	install -m 555 build/kvcachectl /usr/local/bin

deinstall:
	rm /usr/local/bin/kvcached
	rm /usr/local/bin/kvcache-client
	rm /usr/local/bin/kvcachectl

clean:
	rm build/kvcache-client
	rm build/kvcached
	rm build/kvcachectl