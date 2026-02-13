# AI Accelerator Kernel Module Makefile

obj-m := ai_accel.o

# If building multiple source files:
# ai_accel-objs := ai_accel_main.o ai_dma.o ai_ioctl.o

KVERSION ?= $(shell uname -r)
KDIR ?= /lib/modules/$(KVERSION)/build
PWD := $(shell pwd)

# Compiler flags
ccflags-y := -I$(src)/../include

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f Module.symvers modules.order

install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install
	depmod -a

# Load module with simulation enabled
load:
	sudo insmod ai_accel.ko simulate=1

# Unload module
unload:
	sudo rmmod ai_accel

# Reload module
reload: unload load

# Show kernel log
log:
	sudo dmesg | tail -50 | grep ai_accel

# Check if module is loaded
status:
	@lsmod | grep ai_accel || echo "Module not loaded"
	@ls -la /dev/ai_accel 2>/dev/null || echo "Device node not created"

help:
	@echo "AI Accelerator Kernel Module"
	@echo ""
	@echo "Targets:"
	@echo "  all      - Build module"
	@echo "  clean    - Clean build artifacts"
	@echo "  install  - Install module system-wide"
	@echo "  load     - Load module (simulation mode)"
	@echo "  unload   - Unload module"
	@echo "  reload   - Unload and reload module"
	@echo "  log      - Show recent kernel log entries"
	@echo "  status   - Check if module is loaded"
	@echo ""
	@echo "Module Parameters:"
	@echo "  simulate=1    - Enable simulation mode (default)"
	@echo "  simulate=0    - Disable simulation (requires hardware)"
	@echo "  num_engines=4 - Number of compute engines"
	@echo ""
	@echo "Example:"
	@echo "  make && sudo insmod ai_accel.ko simulate=1 num_engines=8"

.PHONY: all clean install load unload reload log status help
