# Generated automatically from Makefile.in by configure.
# $KTH: Makefile.in,v 1.34 1999/03/11 13:50:09 joda Exp $

srcdir		= .
top_srcdir	= ../../..

SHELL		= /bin/sh

CC = gcc
LINK = $(CC)
AR = ar
RANLIB = ranlib
DEFS = -DHAVE_CONFIG_H
CFLAGS =  -g -O2 $(WFLAGS)
WFLAGS = 
LD_FLAGS = 
LIBS = -lutil  
INSTALL = /usr/bin/install -c
INSTALL_PROGRAM = ${INSTALL}
MKINSTALLDIRS = ../../../mkinstalldirs

PROGS = telnet$(EXECSUFFIX)

prefix = /usr/athena
exec_prefix = ${prefix}
bindir = ${exec_prefix}/bin
libdir = ${exec_prefix}/lib
transform=s,x,x,
EXECSUFFIX=

SOURCES=authenc.c commands.c main.c network.c ring.c \
	sys_bsd.c telnet.c terminal.c \
	utilities.c

OBJECTS=authenc.o commands.o main.o network.o ring.o sys_bsd.o \
	telnet.o terminal.o utilities.o

libtop=@libtop@

LIBKRB		= -L../../../lib/krb -lkrb
LIBDES		= -L../../../lib/des -ldes
LIBROKEN	= -L../../../lib/roken -lroken

KLIB=$(LIBKRB) $(LIBDES)


all: $(PROGS)

.c.o:
	$(CC) -c $(DEFS) -I../../../include -I.. -I$(srcdir)/.. $(CFLAGS) $(CPPFLAGS) $<

telnet$(EXECSUFFIX): $(OBJECTS)
	$(LINK) $(LD_FLAGS) $(LDFLAGS) -o $@ $(OBJECTS) -L../libtelnet -ltelnet $(KLIB) $(LIBROKEN) $(LIBS) -ltermcap $(LIBROKEN)

install: all
	$(MKINSTALLDIRS) $(DESTDIR)$(bindir)
	for x in $(PROGS); do \
	  $(INSTALL_PROGRAM) $$x $(DESTDIR)$(bindir)/`echo $$x | sed '$(transform)'`; \
	done

uninstall:
	for x in $(PROGS); do \
	  rm -f $(DESTDIR)$(bindir)/`echo $$x | sed '$(transform)'`; \
	done

TAGS: $(SOURCES)
	etags $(SOURCES)

clean cleandir:
	rm -f *.o *.a telnet$(EXECSUFFIX) \#* *~ core

distclean: clean
	rm -f Makefile *~

.PHONY: all install uninstall clean cleandir distclean
