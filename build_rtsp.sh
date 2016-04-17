#!/bin/sh

gcc -I/usr/local/libevent/include -I/usr/local/libuv/include -L/usr/local/libevent/lib -L/usr/local/libuv/lib rtsp.c -levent_core -luv -lpthread -g -o main
