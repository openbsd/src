# Generated automatically from Makefile.in by configure.
# $KTH: Makefile.in,v 1.23 1999/03/10 19:01:11 joda Exp $

SHELL		= /bin/sh

srcdir		= .
top_srcdir	= ../../..

CC 	= gcc
AR	= ar
RANLIB 	= ranlib
DEFS 	= -DHAVE_CONFIG_H
CFLAGS 	=  -g -O2 $(WFLAGS)
WFLAGS = 

INSTALL = /usr/bin/install -c

prefix 	= /usr/athena

SOURCES = sockbuf.c buffer.c
OBJECTS = $(libcommon_OBJS)

libcommon_OBJS = sockbuf.o buffer.o

LIBNAME = $(LIBPREFIX)common
LIBEXT = a
LIBPREFIX = lib
LIB = $(LIBNAME).$(LIBEXT)

all: $(LIB)

.c.o:
	$(CC) -c -I$(srcdir) -I../../../include $(DEFS) $(CFLAGS) $(CPPFLAGS) $<

$(LIB): $(libcommon_OBJS)
	rm -f $@
	ar cr $@ $(libcommon_OBJS)
	-$(RANLIB) $@

install:

uninstall:

TAGS: $(SOURCES)
	etags $(SOURCES)

clean cleandir:
	rm -f *~ *.o libcommon.a core \#*

distclean: 
	rm -f Makefile

$(OBJECTS): ../../../include/config.h

.PHONY: all install uninstall clean cleandir distclean
