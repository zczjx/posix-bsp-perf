CROSS_COMPILE 	?=

AS		= $(CROSS_COMPILE)as
LD		= $(CROSS_COMPILE)ld
CC		= $(CROSS_COMPILE)gcc
CPP		= $(CROSS_COMPILE)g++
AR		= $(CROSS_COMPILE)ar
NM		= $(CROSS_COMPILE)nm
STRIP		= $(CROSS_COMPILE)strip
OBJCOPY		= $(CROSS_COMPILE)objcopy
OBJDUMP		= $(CROSS_COMPILE)objdump
export AS LD CC CPP AR NM
export STRIP OBJCOPY OBJDUMP

TOPDIR=$(shell pwd)
BIN_DIR=$(TOPDIR)/bin
export TOPDIR
export BIN_DIR

CFLAGS = -Wall  -g -fPIC  -rdynamic
CFLAGS += -I $(shell pwd)/include
LDFLAGS += -lm -lpthread
CFLAGS += -I ${TOPDIR}
export CFLAGS LDFLAGS


ARM_LINUX_BSP_EXYNOS_4412 ?= n
ARM_LINUX_BSP_RPI ?= n

obj-y += common/
ifeq ($(ARM_LINUX_BSP_EXYNOS_4412),y)
obj-y += exynos-4412/
endif

ifeq ($(ARM_LINUX_BSP_RPI),y)
obj-y += rpi/
endif


all : 
	mkdir $(BIN_DIR)
	make -f $(TOPDIR)/Makefile.build

clean:
	rm -f $(shell find -name "*.o")
	rm -rf $(BIN_DIR)

distclean:
	rm -f $(shell find -name "*.o")
	rm -rf $(BIN_DIR)
	
