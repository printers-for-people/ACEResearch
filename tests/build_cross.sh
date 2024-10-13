#!/bin/sh
if test ! -e armv7l-linux-musleabihf-cross; then
	echo "Downloading toolchain..."
	wget https://musl.cc/armv7l-linux-musleabihf-cross.tgz
	tar -xf armv7l-linux-musleabihf-cross.tgz
fi
CC=armv7l-linux-musleabihf-cross/bin/armv7l-linux-musleabihf-gcc
STRIP=armv7l-linux-musleabihf-cross/bin/armv7l-linux-musleabihf-strip
$CC -g -Wall -Werror -Wextra -pedantic -std=c11 -static main.c mjson.c -o main_armv7
$STRIP main_armv7
