CC ?= gcc
CFLAGS ?= -g -Wall -Werror -pthread

TARGET = aesdsocket
SRC = aesdsocket.c
OBJ = $(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJ)
	rm -f /var/tmp/aesdsocketdata

# Cross-compilation support
CROSS_COMPILE ?=
ifeq ($(CROSS_COMPILE),arm-linux-gnueabihf-)
	CC = $(CROSS_COMPILE)gcc
	CFLAGS += -static
endif

.PHONY: all clean
default: all
