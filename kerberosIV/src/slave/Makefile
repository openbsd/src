# Generated automatically from Makefile.in by configure.
# $KTH: Makefile.in,v 1.33 1999/03/10 19:01:17 joda Exp $

SHELL = /bin/sh

srcdir = .

CC = gcc
LINK = $(CC)
AR = ar
RANLIB = ranlib
DEFS = -DHAVE_CONFIG_H -DSBINDIR=\"$(sbindir)\"
CFLAGS =  -g -O2 $(WFLAGS)
WFLAGS = 
LD_FLAGS = 

INSTALL = /usr/bin/install -c
INSTALL_PROGRAM = ${INSTALL}
LIBS = -lutil  
MKINSTALLDIRS = ../mkinstalldirs

prefix = /usr/athena
exec_prefix = ${prefix}
libdir = ${exec_prefix}/lib
libexecdir = ${exec_prefix}/libexec
sbindir = ${exec_prefix}/sbin
transform=s,x,x,
EXECSUFFIX=

PROGS = kpropd$(EXECSUFFIX) \
	kprop$(EXECSUFFIX)

SOURCES = kpropd.c kprop.c

OBJECTS = kpropd.o kprop.o

all: $(PROGS)

Wall:
	make CFLAGS="-g -Wall -Wno-comment -Wmissing-prototypes -Wmissing-declarations -D__USE_FIXED_PROTOTYPES__"

.c.o:
	$(CC) -c $(DEFS) -I../include -I$(srcdir) $(CPPFLAGS) $(CFLAGS) $<

install: all
	$(MKINSTALLDIRS) $(DESTDIR)$(libexecdir)
	for x in $(PROGS); do \
	  $(INSTALL_PROGRAM) $$x $(DESTDIR)$(libexecdir)/`echo $$x | sed '$(transform)'`; \
	done

uninstall:
	for x in $(PROGS); do \
	  rm -f $(DESTDIR)$(libexecdir)/`echo $$x | sed '$(transform)'`; \
	done

TAGS: $(SOURCES)
	etags $(SOURCES)

check:

clean:
	rm -f *.a *.o $(PROGS)

mostlyclean: clean

distclean: clean
	rm -f Makefile *.tab.c *~

realclean: distclean
	rm -f TAGS

kprop$(EXECSUFFIX): kprop.o
	$(LINK) $(LD_FLAGS) $(LDFLAGS) -o $@ kprop.o -L../lib/krb -lkrb -L../lib/des -ldes -L../lib/roken -lroken $(LIBS) -lroken

kpropd$(EXECSUFFIX): kpropd.o
	$(LINK) $(LD_FLAGS) $(LDFLAGS) -o $@ kpropd.o -L../lib/krb -lkrb -L../lib/des -ldes -L../lib/roken -lroken $(LIBS) -lroken

$(OBJECTS): ../include/config.h

.PHONY: all Wall install uninstall check clean mostlyclean distclean realclean
