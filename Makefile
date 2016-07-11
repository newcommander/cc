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
LFLAGS=-lavcodec -lswresample -lavutil -pthread \
       -lz -lrt -lm -levent_core -luv -shared

OBJ=uri.o rtsp.o rtcp.o rtp.o encoder.o frame_operation.o

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

sample:
	$(CC) $(CFLAGS) -L./ -lccstream -pthread sample.c -o main

clean:
	rm -f $(TARGET) *.o main
