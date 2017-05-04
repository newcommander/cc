INCLUDE=-I./ -I/root/work/stream/output/include
LINK=-L./ -L/root/work/stream/output/lib

CC=/usr/local/gcc-4.8.2/bin/gcc
CXX=/usr/local/gcc-4.8.2/bin/g++
CFLAGS=-c -g -Wall -fPIC -pthread
CXXFLAGS=-c -g -Wall -fPIC -pthread
LFLAGS=-pthread -lrt -ljsoncpp

OBJ=main.o node.o task.o action.o learn.o task_1.o action_1.o
TEST_OBJ=test.o

TARGET=run
TEST_TARGET=test

.PHONY: all test clean

%.o: %.c
	$(CC) $(INCLUDE) $(CFLAGS) $< -o $@

%.o: %.cpp
	$(CXX) $(INCLUDE) $(CXXFLAGS) $< -o $@

all: $(OBJ)
	$(CXX) $(LINK) $(OBJ) $(LFLAGS) -o $(TARGET)

test: $(TEST_OBJ)
	$(CXX) $(LINK) $(TEST_OBJ) $(LFLAGS) -o $(TEST_TARGET)

clean:
	rm -f $(OBJ) $(TEST_OBJ) $(TARGET) $(TEST_TARGET)

