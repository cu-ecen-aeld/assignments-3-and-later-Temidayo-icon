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
ifneq ($(CROSS_COMPILE),)
	CC = $(CROSS_COMPILE)gcc
	CFLAGS += -static
endif

# Buildroot cross-compilation support
ifneq ($(TARGET_CC),)
	CC = $(TARGET_CC)
	CFLAGS += -static
endif

# Installation targets for native system
install: $(TARGET)
	install -d /usr/local/bin/
	install -m 755 $(TARGET) /usr/local/bin/
	install -d /etc/init.d/
	install -m 755 aesdsocket-start-stop /etc/init.d/
	@if command -v update-rc.d >/dev/null 2>&1; then \
		update-rc.d aesdsocket-start-stop defaults; \
	elif command -v chkconfig >/dev/null 2>&1; then \
		chkconfig --add aesdsocket-start-stop; \
	else \
		echo "Warning: Could not set up automatic startup (update-rc.d or chkconfig not found)"; \
	fi
	@echo "Installation complete!"
	@echo "Usage: sudo service aesdsocket-start-stop {start|stop|restart|status}"

uninstall:
	@if [ -f /etc/init.d/aesdsocket-start-stop ]; then \
		/etc/init.d/aesdsocket-start-stop stop 2>/dev/null || true; \
	fi
	rm -f /usr/local/bin/aesdsocket 2>/dev/null || true
	rm -f /etc/init.d/aesdsocket-start-stop 2>/dev/null || true
	@if command -v update-rc.d >/dev/null 2>&1; then \
		update-rc.d aesdsocket-start-stop remove 2>/dev/null || true; \
	elif command -v chkconfig >/dev/null 2>&1; then \
		chkconfig --del aesdsocket-start-stop 2>/dev/null || true; \
	fi
	@echo "Uninstall complete!"

.PHONY: all clean install uninstall
default: all
