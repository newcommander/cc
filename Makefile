LIBEVENT=/root/work/stream/output
FFMPEG=/root/work/stream/output
LIBUV=/root/work/stream/output

INCLUDE=-I$(LIBEVENT)/include \
		-I$(FFMPEG)/include \
		-I$(LIBUV)/include

LINK=-L$(LIBEVENT)/lib \
	 -L$(FFMPEG)/lib \
	 -L$(LIBUV)/lib

CC=gcc
CFLAGS=-g -Wall -fPIC
LFLAGS=-lavcodec -lavformat -lswresample -lavutil -pthread \
	   -lz -lrt -lm -levent_core -luv -shared

OBJ=uri.o rtcp.o session.o encoder.o sample_functions.o \
	rtsp.o ccstream.o

TARGET=libccstream.so
INSTALL_DIR=/usr/lib64/

.PHONY: all clean install

%.o: %.c
	$(CC) $(INCLUDE) $(CFLAGS) -c $< -o $@

all: $(OBJ)
	$(CC) $(LINK) $(OBJ) $(LFLAGS) -o $(TARGET)
	rm -f *.o

install:
	cp -f $(TARGET) /usr/lib/
	cp -f $(TARGET) /usr/lib64/

example:
	$(CC) $(CFLAGS) -L. -lccstream -pthread example.c -o main

clean:
	rm -f $(TARGET) *.o main
