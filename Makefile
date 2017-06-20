CCSTREAM_DIR=ccstream
EXAMPLE_DIR=example

INCLUDE=-I/root/work/stream/output/include
LINK=-L/root/work/stream/output/lib -L./$(CCSTREAM_DIR)

CC=/usr/local/gcc-4.8.2/bin/gcc
CXX=/usr/local/gcc-4.8.2/bin/g++
CFLAGS=-c -g -Wall -fPIC -pthread
CXXFLAGS=-c -g -Wall -fPIC -pthread
LFLAGS=-lm -lccstream -lfreetype

OBJ=opencv.o font.o

TARGET=run

.PHONY: all example ccstream clean example_clean ccstream_clean

%.o: %.c
	$(CC) $(INCLUDE) $(CFLAGS) $< -o $@

%.o: %.cpp
	$(CXX) $(INCLUDE) $(CXXFLAGS) $< -o $@

all: ccstream $(OBJ)
	$(CC) $(LINK) $(OBJ) $(LFLAGS) -o $(TARGET)

example: ccstream
	make -C $(EXAMPLE_DIR)

ccstream:
	make -C $(CCSTREAM_DIR)

clean: ccstream_clean example_clean
	rm -f $(OBJ) $(TARGET)

ccstream_clean:
	make -C $(CCSTREAM_DIR) clean

example_clean:
	make -C $(EXAMPLE_DIR) clean
