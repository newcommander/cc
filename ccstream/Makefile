INCLUDE=-I/root/work/stream/output/include
LINK=-L/root/work/stream/output/lib -L.

CC=/usr/local/gcc-4.8.2/bin/gcc
CFLAGS=-c -g -Wall -fPIC -pthread
LFLAGS=-lavcodec -lavformat -lswresample -lavutil -lswscale -pthread \
	   -levent_core -luv -lopencv_imgproc -lz -lrt -lm

OBJ=uri.o rtp.o rtcp.o session.o encoder.o rtsp.o ccstream.o

TARGET=libccstream.so

.PHONY: all clean

%.o: %.c
	$(CC) $(INCLUDE) $(CFLAGS) $< -o $@

all: $(OBJ)
	$(CC) $(LINK) $(OBJ) $(LFLAGS) -shared -o $(TARGET)

clean:
	rm -f $(OBJ) $(TARGET)
