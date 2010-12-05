#
# demo_code makefile
#

TRGTS = userland_mc user_test userland_cpu
SYNTAX_TRGTS = TRGTS
TEST_TRGTS = 

INCLUDES = ../usb_driver

all: CMD = all
all: $(TRGTS) 

clean: CMD = clean
clean: 

check-syntax: CMD = check-syntax
check-syntax: $(SYNTAX_TRGTS)

test: CMD = test
test: $(TEST_TRGTS)

userland_mc: userland_mc.c
	$(CC) -I$(INCLUDES) -g $< -o $@

user_test: user_test.c
	$(CC) -I$(INCLUDES) -g $< -o $@

userland_cpu: userland_cpu.c
	$(CC) -I$(INCLUDES) -g $< -o $@

