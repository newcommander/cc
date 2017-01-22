INCLUDE=-I/root/work/stream/output/include
LINK=-L/root/work/stream/output/lib -L.
EXAMPLE_DIR=example

CC=/usr/local/gcc-4.8.2/bin/gcc
CXX=/usr/local/gcc-4.8.2/bin/g++
CFLAGS=-c -g -Wall -fPIC -pthread
CXXFLAGS=-c -g -Wall -fPIC -pthread
LFLAGS=-lavcodec -lavformat -lswresample -lavutil -pthread \
	   -levent_core -luv -lopencv_imgproc -lz -lrt -lm

OBJ=uri.o rtp.o rtcp.o session.o encoder.o rtsp.o ccstream.o
#EXAMPLE_OBJ=$(EXAMPLE_DIR)/main.o $(EXAMPLE_DIR)/sample_functions.o
EXAMPLE_OBJ=$(EXAMPLE_DIR)/opencv.o
EXAMPLE_TARGET=$(EXAMPLE_DIR)/run

TARGET=libccstream.so
INSTALL_DIR=/usr/lib64/

.PHONY: all example install unstall clean

%.o: %.c
	$(CC) $(INCLUDE) $(CFLAGS) $< -o $@

%.o: %.cpp
	$(CXX) $(INCLUDE) $(CXXFLAGS) $< -o $@

all: $(OBJ)
	$(CC) $(LINK) $(OBJ) $(LFLAGS) -shared -o $(TARGET)
	rm -f $(OBJ)

example: $(EXAMPLE_OBJ)
	$(CC) $(LINK) $(EXAMPLE_OBJ) $(LFLAGS) -lccstream -o $(EXAMPLE_TARGET)
	rm -f $(EXAMPLE_OBJ)

install:
	cp -f $(TARGET) /usr/lib/
	cp -f $(TARGET) /usr/lib64/

unstall:
	rm -f /use/lib/$(TARGET)
	rm -f /use/lib64/$(TARGET)

clean:
	rm -f $(OBJ) $(EXAMPLE_OBJ) $(TARGET) $(EXAMPLE_TARGET)
