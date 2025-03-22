# Variables
CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall -Werror -g -O0
TARGET = writer
SRC = finder-app/writer.c
OBJ = $(SRC:.c=.o)

#Rules
.PHONY: all clean

all: build

#Build target

build: $(TARGET)

# Default target: Build the writer application
#all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

# Compile source file into object file
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Clean target: Remove the writer application and object files
clean:
	rm -f $(TARGET) $(OBJ)

