INCLUDE=-I/root/work/stream/output/include
LINK=-L/root/work/stream/output/lib

MEMORY_DIR=memory
MIND_DIR=mind
PERCEPTRON_DIR=perceptron
ACTION_DIR=action

CC=/usr/local/gcc-4.8.2/bin/gcc
CXX=/usr/local/gcc-4.8.2/bin/g++
CFLAGS=-c -g -Wall -fPIC -pthread
CXXFLAGS=-c -g -Wall -fPIC -pthread
LFLAGS=-pthread -lrt -lopencv_imgproc -lavcodec -lavformat -lswresample -lavutil -lswresample -lswscale -lfreetype -lccstream

OBJ=opencv.o font.o
EXTERN_OBJ=$(MEMORY_DIR)/build.o $(MIND_DIR)/build.o $(PERCEPTRON_DIR)/setup.o $(ACTION_DIR)/setup.o

TARGET=run

.PHONY: all memory_shared mind_shared perceptron_shared action_shared clean

%.o: %.c
	$(CC) $(INCLUDE) $(CFLAGS) $< -o $@

%.o: %.cpp
	$(CXX) $(INCLUDE) $(CXXFLAGS) $< -o $@

all: $(OBJ) memory_shared mind_shared perceptron_shared action_shared
	$(CXX) $(LINK) $(OBJ) $(EXTERN_OBJ) $(LFLAGS) -o $(TARGET)

memory_shared:
	make -C $(MEMORY_DIR) memory_shared

mind_shared:
	make -C $(MIND_DIR) mind_shared

perceptron_shared:
	make -C $(PERCEPTRON_DIR) perceptron_shared

action_shared:
	make -C $(ACTION_DIR) action_shared

clean:
	make -C $(MEMORY_DIR) clean
	make -C $(MIND_DIR) clean
	make -C $(PERCEPTRON_DIR) clean
	make -C $(ACTION_DIR) clean
	rm -f $(TARGET) $(OBJ)

