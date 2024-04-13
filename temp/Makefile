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

CFLAGS ?= -Wall  -g -fPIC  -rdynamic -I $(TOPDIR) -I $(shell pwd)/include -I /usr/include/glib-2.0 -I /usr/lib/aarch64-linux-gnu/glib-2.0/include
LDFLAGS ?= -lm -lpthread -L$(TOPDIR) -lcommon_bsp -lglib-2.0 -lgmodule-2.0 -lgobject-2.0 -lgio-2.0 -lgthread-2.0
export CFLAGS LDFLAGS


ARM_LINUX_BSP_EXYNOS_4412 ?= n
ARM_LINUX_BSP_RPI ?= n
obj-y += common-case/
ifeq ($(ARM_LINUX_BSP_EXYNOS_4412),y)
obj-y += exynos-4412/
endif

ifeq ($(ARM_LINUX_BSP_RPI),y)
obj-y += rpi/
endif

all : lib
	mkdir $(BIN_DIR)
	make -f $(TOPDIR)/case.build.mk

BSP_DLIB ?= libcommon_bsp.so
libobj-y += common-lib/
export BSP_DLIB

lib:
	make -f $(TOPDIR)/lib.build.mk

clean:
	rm -f $(shell find -name "*.o")
	rm -f $(shell find -name "*.so")
	rm -rf $(BIN_DIR)

distclean:
	rm -f $(shell find -name "*.o")
	rm -f $(shell find -name "*.so")
	rm -rf $(BIN_DIR)
	
