CC=/usr/local/gcc-4.8.2/bin/gcc
CXX=/usr/local/gcc-4.8.2/bin/g++
CFLAGS=-c -g -Wall -fPIC -pthread
CXXFLAGS=-c -g -Wall -fPIC -pthread
LFLAGS=-pthread -lrt

OBJ=build.o

TARGET=libmind.so

.PHONY: all ming_shared clean

%.o: %.c
	$(CC) $(INCLUDE) $(CFLAGS) $< -o $@

%.o: %.cpp
	$(CXX) $(INCLUDE) $(CXXFLAGS) $< -o $@

all: mind_shared

mind_shared: $(OBJ)
	$(CC) $(LINK) $(OBJ) $(LFLAGS) -shared -o $(TARGET)

clean:
	rm -f $(TARGET) $(OBJ)

