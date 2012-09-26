#
# libss7: An implementation of Signaling System 7 (SS7)
#
# Written by Mark Spencer <markster@linux-support.net>
#
# Copyright (C) 2001, Linux Support Services, Inc.
# All Rights Reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

CC=gcc
GREP=grep
AWK=awk

OSARCH=$(shell uname -s)
PROC?=$(shell uname -m)

# SONAME version; should be changed on every ABI change
# please don't change it needlessly; it's perfectly fine to have a SONAME
# of 1.0 and a version of 1.4.x
SONAME:=2.0

STATIC_LIBRARY=libss7.a
DYNAMIC_LIBRARY:=libss7.so.$(SONAME)
STATIC_OBJS= \
	isup.o \
	mtp2.o \
	mtp3.o \
	ss7.o \
	ss7_sched.o \
	version.o
DYNAMIC_OBJS= \
	$(STATIC_OBJS)
CFLAGS ?= -g
CFLAGS += -Wall -Werror -Wstrict-prototypes -Wmissing-prototypes
CFLAGS += -fPIC $(LIBSS7_OPT) $(COVERAGE_CFLAGS)
INSTALL_PREFIX=$(DESTDIR)
INSTALL_BASE=/usr
libdir?=$(INSTALL_BASE)/lib
ifneq ($(findstring Darwin,$(OSARCH)),)
  SOFLAGS=$(LDFLAGS) -dynamic -bundle -Xlinker -macosx_version_min -Xlinker 10.4 -Xlinker -undefined -Xlinker dynamic_lookup -force_flat_namespace
  ifeq ($(shell /usr/bin/sw_vers -productVersion | cut -c1-4),10.6)
    SOFLAGS+=/usr/lib/bundle1.o
  endif
  LDCONFIG=/usr/bin/true
else
  SOFLAGS=$(LDFLAGS) -shared -Wl,-h$(DYNAMIC_LIBRARY) $(COVERAGE_LDFLAGS)
  LDCONFIG = /sbin/ldconfig
endif
ifneq (,$(findstring X$(OSARCH)X, XLinuxX XGNU/kFreeBSDX XGNUX))
LDCONFIG_FLAGS=-n
else
ifeq (${OSARCH},FreeBSD)
LDCONFIG_FLAGS=-m
#CFLAGS += -I../zaptel -I../zapata
INSTALL_BASE=/usr/local
endif
endif
ifeq (${OSARCH},SunOS)
#CFLAGS += -DSOLARIS -I../zaptel-solaris
CFLAGS += -DSOLARIS
LDCONFIG =
LDCONFIG_FLAGS = \# # Trick to comment out the period in the command below
#INSTALL_PREFIX = /opt/asterisk  # Uncomment out to install in standard Solaris location for 3rd party code
endif

UTILITIES=parser_debug

ifneq ($(wildcard /usr/include/dahdi/user.h),)
UTILITIES+=ss7test ss7linktest
endif

export SS7VERSION

SS7VERSION:=$(shell GREP=$(GREP) AWK=$(AWK) build_tools/make_version .)

#The problem with sparc is the best stuff is in newer versions of gcc (post 3.0) only.
#This works for even old (2.96) versions of gcc and provides a small boost either way.
#A ultrasparc cpu is really v9 but the stock debian stable 3.0 gcc doesnt support it.
ifeq ($(PROC),sparc64)
PROC=ultrasparc
LIBSS7_OPT = -mtune=$(PROC) -O3 -pipe -fomit-frame-pointer -mcpu=v8
else
  ifneq ($(CODE_COVERAGE),)
    LIBSS7_OPT=
    COVERAGE_CFLAGS=-ftest-coverage -fprofile-arcs
    COVERAGE_LDFLAGS=-ftest-coverage -fprofile-arcs
  else
    LIBSS7_OPT=-O2
  endif
endif

ifeq ($(CPUARCH),i686)
CFLAGS += -m32
SOFLAGS += -m32
endif

all: $(STATIC_LIBRARY) $(DYNAMIC_LIBRARY) $(UTILITIES)

update:
	@if [ -d .svn ]; then \
		echo "Updating from Subversion..." ; \
		fromrev="`svn info | $(AWK) '/Revision: / {print $$2}'`"; \
		svn update | tee update.out; \
		torev="`svn info | $(AWK) '/Revision: / {print $$2}'`"; \
		echo "`date`  Updated from revision $${fromrev} to $${torev}." >> update.log; \
		rm -f .version; \
		if [ `grep -c ^C update.out` -gt 0 ]; then \
			echo ; echo "The following files have conflicts:" ; \
			grep ^C update.out | cut -b4- ; \
		fi ; \
		rm -f update.out; \
	else \
		echo "Not under version control";  \
	fi

install: $(STATIC_LIBRARY) $(DYNAMIC_LIBRARY)
	mkdir -p $(INSTALL_PREFIX)$(libdir)
	mkdir -p $(INSTALL_PREFIX)$(INSTALL_BASE)/include
ifneq (${OSARCH},SunOS)
	install -m 644 libss7.h $(INSTALL_PREFIX)$(INSTALL_BASE)/include
	install -m 755 $(DYNAMIC_LIBRARY) $(INSTALL_PREFIX)$(libdir)
	#if [ -x /usr/sbin/sestatus ] && ( /usr/sbin/sestatus | grep "SELinux status:" | grep -q "enabled"); then /sbin/restorecon -v $(INSTALL_PREFIX)$(libdir)/$(DYNAMIC_LIBRARY); fi
	( cd $(INSTALL_PREFIX)$(libdir) ; ln -sf $(DYNAMIC_LIBRARY) libss7.so)
ifeq ($(SONAME),1.0)
	# Add this link for historic reasons
	( cd $(INSTALL_PREFIX)$(libdir) ; ln -sf $(DYNAMIC_LIBRARY) libss7.so.1)
endif
	install -m 644 $(STATIC_LIBRARY) $(INSTALL_PREFIX)$(libdir)
	if test $$(id -u) = 0; then $(LDCONFIG) $(LDCONFIG_FLAGS) $(INSTALL_PREFIX)$(libdir); fi
else
	install -f $(INSTALL_PREFIX)$(INSTALL_BASE)/include -m 644 libss7.h
	install -f $(INSTALL_PREFIX)$(libdir) -m 755 $(DYNAMIC_LIBRARY)
	( cd $(INSTALL_PREFIX)$(libdir) ; ln -sf $(DYNAMIC_LIBRARY) libss7.so)
ifeq ($(SONAME),1.0)
	# Add this link for historic reasons
	( cd $(INSTALL_PREFIX)$(libdir) ; ln -sf $(DYNAMIC_LIBRARY) libss7.so.1)
endif
	install -f $(INSTALL_PREFIX)$(libdir) -m 644 $(STATIC_LIBRARY)
endif

uninstall:
	@echo "Removing Libss7"
	rm -f $(INSTALL_PREFIX)$(libdir)/$(STATIC_LIBRARY)
	rm -f $(INSTALL_PREFIX)$(libdir)/libss7.so
ifeq ($(SONAME),1.0)
	rm -f $(INSTALL_PREFIX)$(libdir)/libss7.so.1
endif
	rm -f $(INSTALL_PREFIX)$(libdir)/$(DYNAMIC_LIBRARY)
	rm -f $(INSTALL_PREFIX)$(INSTALL_BASE)/include/libss7.h

ss7test: ss7test.o $(STATIC_LIBRARY)
	$(CC) -o $@ $< $(STATIC_LIBRARY) -lpthread $(CFLAGS)

ss7linktest: ss7linktest.o $(STATIC_LIBRARY)
	$(CC) -o $@ $< $(STATIC_LIBRARY) -lpthread $(CFLAGS)

parser_debug: parser_debug.o $(STATIC_LIBRARY)
	$(CC) -o $@ $< $(STATIC_LIBRARY) $(CFLAGS)

MAKE_DEPS= -MD -MT $@ -MF .$(subst /,_,$@).d -MP

%.o: %.c
	$(CC) $(CFLAGS) $(MAKE_DEPS) -c -o $@ $<

%.lo: %.c
	$(CC) $(CFLAGS) $(MAKE_DEPS) -c -o $@ $<

$(STATIC_LIBRARY): $(STATIC_OBJS)
	ar rcs $(STATIC_LIBRARY) $(STATIC_OBJS)
	ranlib $(STATIC_LIBRARY)

$(DYNAMIC_LIBRARY): $(DYNAMIC_OBJS)
	$(CC) $(SOFLAGS) -o $@ $(DYNAMIC_OBJS)
	$(LDCONFIG) $(LDCONFIG_FLAGS) .
	ln -sf $(DYNAMIC_LIBRARY) libss7.so
ifeq ($(SONAME),1.0)
	# Add this link for historic reasons
	ln -sf $(DYNAMIC_LIBRARY) libss7.so.1
endif

version.c: FORCE
	@build_tools/make_version_c > $@.tmp
	@cmp -s $@.tmp $@ || mv $@.tmp $@
	@rm -f $@.tmp

clean:
	rm -f *.o *.so *.lo
ifeq ($(SONAME),1.0)
	rm -f *.so.1
endif
	rm -f $(STATIC_LIBRARY) $(DYNAMIC_LIBRARY)
	rm -f parser_debug ss7linktest ss7test
	rm -f .*.d

.PHONY:

FORCE:

ifneq ($(wildcard .*.d),)
   include .*.d
endif
