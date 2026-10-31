
obj-m += mm_ringbuf.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)
CC   := gcc
CFLAGS := -O2 -Wall

MODULE = mm_ringbuf.ko
USER_APP = user_reader

.PHONY: all clean load unload

all: $(USER_APP)
	$(MAKE) -C $(KDIR) M=$(PWD) modules

$(USER_APP): user_reader.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f $(USER_APP)

load: all
	@sudo insmod $(MODULE) && echo "Module loaded successfully." || echo "Load failed."
	@dmesg | tail -n 10

unload:
	@sudo rmmod mm_ringbuf && echo "Module unloaded successfully." || echo "Unload failed."

install: mm_ringbuf.ko
	install mm_ringbuf.ko /lib/modules/$(uname -r)
	install mm_ringbuf.conf /etc/modules-load.d
	install 99-mm_ringbuf.rules /etc/udev/rules.d