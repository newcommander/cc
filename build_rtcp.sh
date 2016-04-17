#!/bin/sh

gcc -I/usr/local/libuv/include -L/usr/local/libuv/lib rtcp.c -luv -Wall -g -o main
