# Generated automatically from Makefile.in by configure.
# $KTH: Makefile.in,v 1.30 1999/03/10 19:01:17 joda Exp $

SHELL = /bin/sh

srcdir = .

CC = gcc
LINK = $(CC)
AR = ar
RANLIB = ranlib
DEFS = -DHAVE_CONFIG_H
CFLAGS =  -g -O2 $(WFLAGS)
WFLAGS = 
LD_FLAGS = 

INSTALL = /usr/bin/install -c
INSTALL_PROGRAM = ${INSTALL}
MKINSTALLDIRS = ../mkinstalldirs

LIBS = -lutil  
LIB_DBM = 

prefix = /usr/athena
exec_prefix = ${prefix}
libdir = ${exec_prefix}/lib
libexecdir = ${exec_prefix}/libexec
transform=s,x,x,
EXECSUFFIX=

PROGS = kerberos$(EXECSUFFIX)

SOURCES = kerberos.c

OBJECTS = kerberos.o

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

kerberos$(EXECSUFFIX): kerberos.o
	$(LINK) $(LD_FLAGS) $(LDFLAGS) -o $@ kerberos.o -L../lib/kdb -lkdb -L../lib/krb -lkrb -L../lib/des -ldes -L../lib/roken -lroken $(LIB_DBM) $(LIBS) -lroken

$(OBJECTS): ../include/config.h

.PHONY: all Wall install uninstall check clean mostlyclean distclean realclean
