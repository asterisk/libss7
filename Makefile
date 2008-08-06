CC=gcc
GREP=grep
AWK=awk

OSARCH=$(shell uname -s)

INSTALL_PREFIX=$(DESTDIR)
INSTALL_BASE=/usr
libdir?=$(INSTALL_BASE)/lib
STATIC_OBJS=mtp2.o ss7_sched.o ss7.o mtp3.o isup.o version.o
DYNAMIC_OBJS=mtp2.o ss7_sched.o ss7.o mtp3.o isup.o version.o
STATIC_LIBRARY=libss7.a
DYNAMIC_LIBRARY=libss7.so.1.0
CFLAGS=-Wall -Werror -Wstrict-prototypes -Wmissing-prototypes -g -fPIC
LDCONFIG_FLAGS=-n
SOFLAGS=-Wl,-hlibss7.so.1
LDCONFIG=/sbin/ldconfig

UTILITIES=parser_debug

ifneq ($(wildcard /usr/include/dahdi/user.h),)
UTILITIES+=ss7test ss7linktest
endif

export SS7VERSION

SS7VERSION:=$(shell GREP=$(GREP) AWK=$(AWK) build_tools/make_version .)

all: $(STATIC_LIBRARY) $(DYNAMIC_LIBRARY) $(UTILITIES)

MAKE_DEPS= -MD -MT $@ -MF .$(subst /,_,$@).d -MP

%.o: %.c
	$(CC) $(CFLAGS) $(MAKE_DEPS) -c -o $@ $<

%.lo: %.c
	$(CC) $(CFLAGS) $(MAKE_DEPS) -c -o $@ $<

clean:
	rm -f *.o *.so *.lo *.so.1 *.so.1.0
	rm -f parser_debug ss7linktest ss7test $(STATIC_LIBRARY) $(DYNAMIC_LIBRARY)
	rm -f .*.d

install: $(STATIC_LIBRARY) $(DYNAMIC_LIBRARY)
	mkdir -p $(INSTALL_PREFIX)$(libdir)
	mkdir -p $(INSTALL_PREFIX)$(INSTALL_BASE)/include
	install -m 644 libss7.h $(INSTALL_PREFIX)$(INSTALL_BASE)/include
	install -m 755 $(DYNAMIC_LIBRARY) $(INSTALL_PREFIX)$(libdir)
	( cd $(INSTALL_PREFIX)$(libdir) ; ln -sf libss7.so.1 libss7.so ; ln -sf libss7.so.1.0 libss7.so.1 )
	install -m 644 $(STATIC_LIBRARY) $(INSTALL_PREFIX)$(libdir)
	if test $$(id -u) = 0; then $(LDCONFIG); fi

$(STATIC_LIBRARY): $(STATIC_OBJS)
	ar rcs $(STATIC_LIBRARY) $(STATIC_OBJS)
	ranlib $(STATIC_LIBRARY)

$(DYNAMIC_LIBRARY): $(DYNAMIC_OBJS)
	$(CC) -shared $(SOFLAGS) -o $@ $(DYNAMIC_OBJS)
	$(LDCONFIG) $(LDCONFIG_FLAGS) .
	ln -sf libss7.so.1 libss7.so
	ln -sf libss7.so.1.0 libss7.so.1

version.c: FORCE
	@build_tools/make_version_c > $@.tmp
	@cmp -s $@.tmp $@ || mv $@.tmp $@
	@rm -f $@.tmp

ss7test: ss7test.c $(STATIC_LIBRARY)
	gcc -g -o ss7test ss7test.c libss7.a -lpthread

ss7linktest: ss7linktest.c $(STATIC_LIBRARY)
	gcc -g -o ss7linktest ss7linktest.c libss7.a -lpthread

parser_debug: parser_debug.c $(STATIC_LIBRARY)
	gcc -g -Wall -o parser_debug parser_debug.c libss7.a

libss7: ss7_mtp.o mtp.o ss7.o ss7_sched.o

.PHONY:

FORCE:

ifneq ($(wildcard .*.d),)
   include .*.d
endif
