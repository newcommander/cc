CC=/usr/local/gcc-4.8.2/bin/gcc
CXX=/usr/local/gcc-4.8.2/bin/g++
CFLAGS=-c -g -Wall -fPIC -pthread
CXXFLAGS=-c -g -Wall -fPIC -pthread
LFLAGS=-pthread -lrt

OBJ=setup.o

TARGET=libaction.so

.PHONY: all action_shared clean

%.o: %.c
	$(CC) $(INCLUDE) $(CFLAGS) $< -o $@

%.o: %.cpp
	$(CXX) $(INCLUDE) $(CXXFLAGS) $< -o $@

all: action_shared

action_shared: $(OBJ)
	$(CC) $(LINK) $(OBJ) $(LFLAGS) -shared -o $(TARGET)

clean:
	rm -f $(TARGET) $(OBJ)

