# Generated automatically from Makefile.in by configure.
# $KTH: Makefile.in,v 1.38 1999/03/11 13:50:16 joda Exp $

srcdir		= .
top_srcdir	= ../../..

top_builddir		= ../../..

SHELL		= /bin/sh

CC = gcc
LINK = $(CC)
AR = ar
RANLIB = ranlib
DEFS = -DHAVE_CONFIG_H -DBINDIR='"$(bindir)"'
CFLAGS =  -g -O2 $(WFLAGS)
WFLAGS = 
LD_FLAGS = 
LIBS = -lutil  
INSTALL = /usr/bin/install -c
INSTALL_PROGRAM = ${INSTALL}
MKINSTALLDIRS = ../../../mkinstalldirs

prefix = /usr/athena
exec_prefix = ${prefix}
bindir = ${exec_prefix}/bin
libdir = ${exec_prefix}/lib
libexecdir = ${exec_prefix}/libexec
transform=s,x,x,
EXECSUFFIX=

PROGS = telnetd$(EXECSUFFIX)

SOURCES=telnetd.c state.c termstat.c slc.c sys_term.c \
	utility.c global.c authenc.c

OBJECTS=telnetd.o state.o termstat.o slc.o sys_term.o \
	utility.o global.o authenc.o

libtop = @libtop@

LIBKRB		= -L../../../lib/krb -lkrb
LIBDES		= -L../../../lib/des -ldes
LIBKAFS		= -L$(top_builddir)/lib/kafs -lkafs 
LIBROKEN	= -L../../../lib/roken -lroken

KLIB=$(LIBKAFS) $(LIBKRB) $(LIBDES)


all: $(PROGS)

.c.o:
	$(CC) -c $(DEFS) -I../../../include -I.. -I$(srcdir)/.. -I. -I$(srcdir) $(CFLAGS) $(CPPFLAGS) $<

telnetd$(EXECSUFFIX): $(OBJECTS)
	$(LINK) $(LD_FLAGS) $(LDFLAGS) -o $@ $(OBJECTS) -L../libtelnet -ltelnet $(KLIB) $(LIBROKEN) $(LIBS) -ltermcap $(LIBROKEN)

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

clean cleandir:
	rm -f *.o *.a telnetd$(EXECSUFFIX) \#* *~ core

distclean: clean
	rm -f Makefile *~


.PHONY: all install uninstall clean cleandir distclean
