#!/bin/sh

gcc -I/usr/local/libevent/include -I/usr/local/libuv/include -L/usr/local/libevent/lib -L/usr/local/libuv/lib uri.c rtsp.c rtp.c -levent_core -luv -lpthread -g -o main
