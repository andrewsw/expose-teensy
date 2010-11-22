#
# general project wide makefile
#

TRGTS = DRIVER
SYNTAX_TRGTS = DRIVER
TEST_TRGTS = 

all: CMD = all
all: $(TRGTS) 

clean: CMD = clean
clean: $(TRGTS)

check-syntax: CMD = check-syntax
check-syntax: $(SYNTAX_TRGTS)

test: CMD = test
test: $(TEST_TRGTS)

DRIVER:
	cd ./usb_driver; $(MAKE) $(CMD)

userland_mc: userland_mc.c
	$(CC) -g $< -o $@

teensy_usb_hw/example.hex:
	cd teensy_usb_hw && make

# you are NOT expected to compile teensy_loader_cli; just use the precompiled version
load-hw: teensy_usb_hw/example.hex
	teensy_loader/teensy_loader_cli -mmcu=atmega32u4 -w -v teensy_usb_hw/example.hex

load:
	insmod usb_driver/teensy_mono.ko

unload:
	rmmod teensy_mono

reload: unload load

