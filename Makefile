OBJS := $(wildcard *.cpp src/*.cpp src/graphics/*.cpp src/objects/*.cpp)
BINS := $(SRCS:%.cpp=%)

CC = g++ -std=c++17 -O2 -w
COUNT = cloc
LD_FLAGS = src/includes/glad.c -ldl -lglfw -lz -lglut -lGL -lGLU -lSDL2

OBJ_NAME = cryogen.exe

.PHONY: build clean

build : ${BINS}
	$(CC) $(OBJS) $(LD_FLAGS) -o $(OBJ_NAME)

run : ${BINS}
	$(CC) $(OBJS) $(LD_FLAGS) -o $(OBJ_NAME)
	./${OBJ_NAME}

clean:
	rm -f $(OBJ_NAME)