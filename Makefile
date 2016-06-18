LIBEVENT=/usr/local/libevent
FFMPEG=/usr/local/ffmpeg
LIBUV=/usr/local/libuv

INCLUDE=-I$(LIBEVENT)/include \
		-I$(FFMPEG)/include \
		-I$(LIBUV)/include

LINK=-L$(LIBEVENT)/lib \
	 -L$(FFMPEG)/lib \
	 -L$(LIBUV)/lib

CC=gcc
CFLAGS=-g -Wall
LFLAGS=-lavcodec -lswresample -lavutil -lpthread \
	   -lz -lrt -lm -levent_core -luv

OBJ=uri.o rtsp.o rtcp.o rtp.o encoding.o frame_opreation.o

TARGET=main

.PHONY: all clean

%.o: %.c
	$(CC) $(INCLUDE) $(CFLAGS) -c $< -o $@

all: $(OBJ)
	$(CC) $(LINK) $(OBJ) $(LFLAGS) -o $(TARGET)
	rm -f *.o

clean:
	rm -f $(TARGET) *.o
