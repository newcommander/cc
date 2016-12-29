INCLUDE=-I/root/work/stream/output/include
LINK=-L/root/work/stream/output/lib -L.

CC=gcc
CFLAGS=-g -Wall -fPIC -pthread
LFLAGS=-lavcodec -lavformat -lswresample -lavutil -pthread \
	   -lz -lrt -lm -levent_core -luv

OBJ=uri.o rtp.o rtcp.o session.o encoder.o rtsp.o ccstream.o
EXAMPLE_OBJ=example.o

TARGET=libccstream.so
INSTALL_DIR=/usr/lib64/

.PHONY: all example install unstall clean

%.o: %.c
	$(CC) $(INCLUDE) $(CFLAGS) -c $< -o $@

all: $(OBJ)
	$(CC) $(LINK) $(OBJ) $(LFLAGS) -shared -o $(TARGET)
	rm -f *.o

example: $(EXAMPLE_OBJ)
	$(CC) $(LINK) $(EXAMPLE_OBJ) $(LFLAGS) -lccstream -o main
	rm -f *.o

install:
	cp -f $(TARGET) /usr/lib/
	cp -f $(TARGET) /usr/lib64/

unstall:
	rm -f /use/lib/$(TARGET)
	rm -f /use/lib64/$(TARGET)

clean:
	rm -f $(TARGET) *.o main
