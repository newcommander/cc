INCLUDE=-I./ -I/root/work/stream/output/include
LINK=-L./ -L/root/work/stream/output/lib

CC=/usr/local/gcc-4.8.2/bin/gcc
CXX=/usr/local/gcc-4.8.2/bin/g++
CFLAGS=-c -g -Wall -fPIC -pthread
CXXFLAGS=-c -g -Wall -fPIC -pthread
LFLAGS=-lm -ljsoncpp

OBJ=main.o

TARGET=run

.PHONY: all clean

%.o: %.c
	$(CC) $(INCLUDE) $(CFLAGS) $< -o $@

%.o: %.cpp
	$(CXX) $(INCLUDE) $(CXXFLAGS) $< -o $@

all: $(OBJ)
	$(CXX) $(LINK) $(OBJ) $(LFLAGS) -o $(TARGET)

clean:
	rm -f $(OBJ) $(TARGET)

