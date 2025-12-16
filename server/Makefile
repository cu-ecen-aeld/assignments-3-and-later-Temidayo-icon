CC ?= gcc
CFLAGS ?= -Wall -Werror -g

TARGET = aesdsocket
SRC = aesdsocket.c

all: $(TARGET)

default: all

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET)

