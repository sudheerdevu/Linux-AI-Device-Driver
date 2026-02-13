# Userspace test programs Makefile

CC = gcc
CFLAGS = -Wall -Wextra -I../include

TARGETS = test_driver

.PHONY: all clean

all: $(TARGETS)

test_driver: test_driver.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(TARGETS)
