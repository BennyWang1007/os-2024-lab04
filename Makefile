KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

obj-m += osfs.o

osfs-objs := super.o inode.o file.o dir.o osfs_init.o

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	$(MAKE) unmount
	$(MAKE) unload_mod

test:
	gcc generate_file.c -o gen
	./gen mnt/test.txt 10000
	cat mnt/test.txt
	echo "len:"
	awk '{print length}' mnt/test.txt

mount:
	sudo mount -t osfs None mnt
unmount:
	sudo umount mnt
load_mod:
	sudo insmod osfs.ko
unload_mod:
	sudo rmmod osfs

init:
	$(MAKE) load_mod
	$(MAKE) mount
	sudo dmesg | grep "osfs: Successfully registered"

reinit:
	$(MAKE) unmount
	$(MAKE) unload_mod
	$(MAKE) init

reall:
	$(MAKE) unmount
	$(MAKE) unload_mod
	$(MAKE) all
	$(MAKE) init




