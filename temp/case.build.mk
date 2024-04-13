PHONY := __build
__build:

LOCAL_LDFLAGS :=

obj-y :=
test-bin :=
subdir-y :=


include Makefile

# obj-y := a.o b.o c/ d/
# $(filter %/, $(obj-y))   : c/ d/
# __subdir-y  : c d
# subdir-y    : c d
__subdir-y	:= $(patsubst %/,%,$(filter %/, $(obj-y)))
subdir-y	+= $(__subdir-y)


# a.o b.o
test-objs := $(filter-out %/, $(obj-y))

PHONY += $(subdir-y)

__build : $(subdir-y) $(test-bin)


$(subdir-y):
	make -C $@ -f $(TOPDIR)/case.build.mk
	
$(test-bin) : $(test-objs)
	$(CC) -o $@ $^  $(LDFLAGS) $(LOCAL_LDFLAGS)
	mv $@ $(BIN_DIR)

%.o : %.c
	$(CC)  -fPIC $(CFLAGS) -c -o $@ $<
	
.PHONY : $(PHONY)

