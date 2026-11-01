CROSS_COMPILE ?= mipsel-linux-gnu-
CC = $(CROSS_COMPILE)gcc
STRIP = $(CROSS_COMPILE)strip

VENDOR_DIR ?= ../third_party/Hiby-R1-Mod/bidhata-menu
CPPFLAGS += -Iinclude -I$(VENDOR_DIR)/include
CFLAGS += -O2 -Wall -Wextra -std=c11 -D_GNU_SOURCE \
          -march=mips32r2 -mtune=mips32r2 -fno-pie
LDFLAGS += -static -no-pie

SRCS = src/main.c src/executor.c src/keyboard.c src/tools.c src/connectivity.c \
       $(VENDOR_DIR)/src/platform.c $(VENDOR_DIR)/src/font.c
OBJS = $(SRCS:.c=.o)
HELPERS = geos-dump geos-mon

all: geos-cyber helpers

geos-cyber: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^
	$(STRIP) $@

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

geos-dump: helpers/geos-dump.c
	$(CC) $(CPPFLAGS) -Os -Wall -D_GNU_SOURCE -static -o $@ $<
	$(STRIP) $@

geos-mon: helpers/geos-mon.c
	$(CC) $(CPPFLAGS) -Os -Wall -D_GNU_SOURCE -static -o $@ $<
	$(STRIP) $@

capture: src/capture.c
	$(CC) $(CPPFLAGS) -Os -Wall -D_GNU_SOURCE -static -o $@ $<
	$(STRIP) $@

clean:
	rm -f $(OBJS) geos-cyber $(HELPERS) capture

.PHONY: all clean helpers
