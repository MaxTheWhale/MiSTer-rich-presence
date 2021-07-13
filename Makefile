# using gcc version 7.5.0 (Linaro GCC 7.5-2019.12)
BASE    = /opt/gcc-linaro-7.5.0-2019.12-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf

CC      = $(BASE)-gcc
LD      = $(BASE)-ld

CFLAGS  = -Wall -pedantic -std=c11 -pthread

notify: notify.c
	$(CC) $(CFLAGS) -o $@ $<
