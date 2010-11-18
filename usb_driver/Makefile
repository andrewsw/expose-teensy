# usb_driver module specific makefile

obj-m += teensy_mono.o
# http://linux.die.net/lkmpg/x351.html
teensy_mono-objs := teensy.o submodules.o teensy_adc.o teensy_mc.o

# if kernel version is not 2.6.24 then assume 2.6.24 kernel source is
# in /root
ifneq "$(shell uname -r | grep -o '^2\.6\.24')" "2.6.24"
KERNEL_ENV=$(HOME)/src/linux-2.6.24/
else
KERNEL_ENV=/lib/modules/$(shell uname -r)/build
endif

all: 
	make -C $(KERNEL_ENV) M=$(PWD) modules

clean:
	make -C $(KERNEL_ENV) M=$(PWD) clean

check-syntax: 
	make -C $(KERNEL_ENV) M=$(PWD) modules

test: 
	# put in test code here...

