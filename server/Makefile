# using gcc version 7.5.0 (Linaro GCC 7.5-2019.12)
BASE    = /opt/gcc-linaro-7.5.0-2019.12-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf
CC      = $(BASE)-gcc
CFLAGS  = -Wall -pedantic -std=c11

SOURCES = $(wildcard *.c)

mister_status: *.c
	$(CC) $(CFLAGS) -o $@ $(SOURCES)
