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
	gcc-4.2 -g $< -o $@

load:
	insmod usb_driver/teensy_mono.ko

unload:
	rmmod teensy_mono

reload: unload load

