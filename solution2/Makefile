INCLUDE=-I./ -I/root/work/stream/output/include
LINK=-L./ -L/root/work/stream/output/lib

MEMORY_DIR=memory
MIND_DIR=mind
PERCEPTRON_DIR=perceptron
ACTION_DIR=action

CC=/usr/local/gcc-4.8.2/bin/gcc
CXX=/usr/local/gcc-4.8.2/bin/g++
CFLAGS=-c -g -Wall -fPIC -pthread
CXXFLAGS=-c -g -Wall -fPIC -pthread
LFLAGS=-pthread -lrt

OBJ=startup.o
EXTERN_OBJ=$(MEMORY_DIR)/build.o $(MIND_DIR)/build.o $(PERCEPTRON_DIR)/setup.o $(ACTION_DIR)/setup.o

TARGET=run

.PHONY: all memory mind perceptron action clean

%.o: %.c
	$(CC) $(INCLUDE) $(CFLAGS) $< -o $@

%.o: %.cpp
	$(CXX) $(INCLUDE) $(CXXFLAGS) $< -o $@

all: $(OBJ) memory mind perceptron action
	$(CXX) $(LINK) $(OBJ) $(EXTERN_OBJ) $(LFLAGS) -o $(TARGET)

memory:
	make -C $(MEMORY_DIR)

mind:
	make -C $(MIND_DIR)

perceptron:
	make -C $(PERCEPTRON_DIR)

action:
	make -C $(ACTION_DIR)

clean:
	make -C $(MEMORY_DIR) clean
	make -C $(MIND_DIR) clean
	make -C $(PERCEPTRON_DIR) clean
	make -C $(ACTION_DIR) clean
	rm -f $(TARGET) $(OBJ)

