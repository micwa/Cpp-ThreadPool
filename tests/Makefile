# Compiler and source extension
CC = g++
EXT = cpp

OBJS =  tp_test.o
CFLAGS = -O2 -Wall -Wpedantic -Wextra -Wshadow -Wfloat-equal -std=c++11 -lpthread $(INCLUDES)
TARGET = main.out
.PHONY : all clean

all : $(TARGET)

$(TARGET) : $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o : %.$(EXT)
	$(CC) $(CFLAGS) -o $@ -c $^

clean :
	-rm $(TARGET) *.o
