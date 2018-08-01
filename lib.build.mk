PHONY := __build
__build:


libobj-y :=
bsp_dlib := $(BSP_DLIB)
subdir-y :=

include Makefile

# libobj-y := a.o b.o c/ d/
# $(filter %/, $(libobj-y))   : c/ d/
# __subdir-y  : c d
# subdir-y    : c d
__subdir-y	:= $(patsubst %/,%,$(filter %/, $(libobj-y)))
subdir-y	+= $(__subdir-y)
BSP_DLIB_LDFLAGS := -lm -lpthread

# a.o b.o
dlib-objs := $(filter-out %/, $(libobj-y))

PHONY += $(subdir-y)

__build : $(subdir-y) $(bsp_dlib)


$(subdir-y):
	make -C $@ -f $(TOPDIR)/lib.build.mk
	
$(bsp_dlib) : $(dlib-objs)
	${CC}  $(BSP_DLIB_LDFLAGS)  -shared $^ -o $@
	mv $@ $(TOPDIR)

%.o : %.c
	$(CC)  -fPIC $(CFLAGS) -c -o $@ $<
	
.PHONY : $(PHONY)
