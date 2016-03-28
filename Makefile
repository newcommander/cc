CC=gcc

FFMPEG=/usr/local/ffmpeg

INCLUDE = -I$(FFMPEG)/include
LINK = -L$(FFMPEG)/lib

TARGET=main

CFLAGS=-g
LFLAGS=-lavcodec -lswresample -lavutil \
	   -lz -lrt -lm

.PHONY: all clean

all:
	$(CC) $(INCLUDE) $(LINK) encoding.c $(CFLAGS) $(LFLAGS) -o $(TARGET)

clean:
	rm -f $(TARGET)
