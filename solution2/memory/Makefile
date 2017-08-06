CC=/usr/local/gcc-4.8.2/bin/gcc
CXX=/usr/local/gcc-4.8.2/bin/g++
CFLAGS=-c -g -Wall -fPIC -pthread
CXXFLAGS=-c -g -Wall -fPIC -pthread
LFLAGS=-pthread -lrt

OBJ=build.o
TARGET=libmemory.so

TEST_OBJ=test.o
TEST_TARGET=run

.PHONY: all memory_shared test clean

%.o: %.c
	$(CC) $(INCLUDE) $(CFLAGS) $< -o $@

%.o: %.cpp
	$(CXX) $(INCLUDE) $(CXXFLAGS) $< -o $@

all: memory_shared test
	$(CC) $(LINK) $(OBJ) $(TEST_OBJ) $(LFLAGS) -o $(TEST_TARGET)

memory_shared: $(OBJ)
	$(CC) $(LINK) $(OBJ) $(LFLAGS) -shared -o $(TARGET)

test: $(TEST_OBJ)

clean:
	rm -f $(TARGET) $(TEST_TARGET) $(OBJ) $(TEST_OBJ)

