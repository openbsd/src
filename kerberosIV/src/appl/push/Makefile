# Generated automatically from Makefile.in by configure.
# $KTH: Makefile.in,v 1.10 1999/04/07 18:39:56 assar Exp $

SHELL = /bin/sh

srcdir = .
top_srcdir = ../..

CC = gcc
LINK = $(CC)
RANLIB = ranlib
DEFS = -DHAVE_CONFIG_H 
CFLAGS =  -g -O2 $(WFLAGS)
WFLAGS = 
LD_FLAGS = 
INSTALL = /usr/bin/install -c
INSTALL_PROGRAM = ${INSTALL}
LIBS =  -lutil  
LIB_DBM = 
MKINSTALLDIRS = ../../mkinstalldirs

prefix = /usr/athena
exec_prefix = ${prefix}
libdir = ${exec_prefix}/lib
libexecdir = ${exec_prefix}/libexec
bindir = ${exec_prefix}/bin
transform=s,x,x,
EXECSUFFIX=

bin_PROGRAMS		= pfrom

libexec_PROGRAMS	= push$(EXECSUFFIX)

PROGRAMS		= $(libexec_PROGRAMS) $(bin_PROGRAMS)

push_SOURCES		= push.c

push_OBJECTS		= push.o

SOURCES			= $(push_SOURCES)

OBJECTS			= $(push_OBJECTS)

all: $(PROGRAMS)

.c.o:
	$(CC) -c $(DEFS) -I../../include -I$(srcdir) $(CFLAGS) $(CPPFLAGS) $<

install: all
	$(MKINSTALLDIRS) $(DESTDIR)$(bindir)
	for x in $(bin_PROGRAMS); do \
	  $(INSTALL_PROGRAM) $$x $(DESTDIR)$(bindir)/`echo $$x | sed '$(transform)'`; \
	done
	$(MKINSTALLDIRS) $(DESTDIR)$(libexecdir)
	for x in $(libexec_PROGRAMS); do \
	  $(INSTALL_PROGRAM) $$x $(DESTDIR)$(libexecdir)/`echo $$x | sed '$(transform)'`; \
	done

uninstall:
	for x in $(bin_PROGRAMS); do \
	  rm -f $(DESTDIR)$(bindir)/`echo $$x | sed '$(transform)'`; \
	done
	for x in $(libexec_PROGRAMS); do \
	  rm -f $(DESTDIR)$(libexecdir)/`echo $$x | sed '$(transform)'`; \
	done

TAGS: $(SOURCES)
	etags $(SOURCES)

check:

clean:
	rm -f *.a *.o $(PROGRAMS)

mostlyclean: clean

distclean: clean
	rm -f Makefile *~

realclean: distclean
	rm -f TAGS

KLIB=-L../../lib/krb -lkrb -L../../lib/des -ldes
LIBROKEN=-L../../lib/roken -lroken

push$(EXECSUFFIX): $(push_OBJECTS)
	$(LINK) $(LD_FLAGS) $(LDFLAGS) -o $@ $(push_OBJECTS) $(KLIB) $(LIBROKEN) $(LIBS) $(LIBROKEN)

pfrom: pfrom.in
	sed -e "s!%libexecdir%!$(libexecdir)!" $(srcdir)/pfrom.in > $@
	chmod +x $@

$(OBJECTS): ../../include/config.h

.PHONY: all install uninstall check clean mostlyclean distclean realclean
