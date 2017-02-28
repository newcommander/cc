INCLUDE=-I/root/work/stream/output/include
LINK=-L/root/work/stream/output/lib -L../ccstream

CC=/usr/local/gcc-4.8.2/bin/gcc
CXX=/usr/local/gcc-4.8.2/bin/g++
CFLAGS=-c -g -Wall -fPIC -pthread
CXXFLAGS=-c -g -Wall -fPIC -pthread
LFLAGS=-lccstream

OBJ=main.o sample_functions.o

TARGET=a.out

.PHONY: all clean

%.o: %.c
	$(CC) $(INCLUDE) $(CFLAGS) $< -o $@

%.o: %.cpp
	$(CXX) $(INCLUDE) $(CXXFLAGS) $< -o $@

all: $(OBJ)
	$(CC) $(LINK) $(OBJ) $(LFLAGS) -o $(TARGET)

clean:
	rm -f $(OBJ) $(TARGET)
