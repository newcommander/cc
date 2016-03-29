#!/bin/sh

gcc -I/usr/local/libevent/include -L/usr/local/libevent/lib rtsp.c -levent_core -g -o main
