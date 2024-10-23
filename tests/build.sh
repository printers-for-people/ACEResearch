#!/bin/sh
if test ! -e main.c; then
	echo "Run this script from the tests directory"
	exit 1
fi
gcc -g -Wall -Werror -Wextra -pedantic -std=c11 main.c mjson.c -o main
