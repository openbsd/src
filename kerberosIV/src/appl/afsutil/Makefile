# Generated automatically from Makefile.in by configure.
# $KTH: Makefile.in,v 1.27 1999/03/10 19:01:10 joda Exp $

SHELL	= /bin/sh

srcdir	= .

top_builddir	= ../..

CC	= gcc
LINK	= $(CC)
AR	= ar
RANLIB	= ranlib
DEFS	= -DHAVE_CONFIG_H
CFLAGS	=  -g -O2 $(WFLAGS)
WFLAGS = 
LD_FLAGS= 
INSTALL	= /usr/bin/install -c
INSTALL_PROGRAM = ${INSTALL}
LIBROKEN = -L../../lib/roken -lroken
LIBS	= -L$(top_builddir)/lib/kafs -lkafs  -L../../lib/krb -lkrb -L../../lib/des -ldes $(LIBROKEN) -lutil   $(LIBROKEN)
MKINSTALLDIRS = ../../mkinstalldirs

prefix = /usr/athena
exec_prefix = ${prefix}
libdir = ${exec_prefix}/lib
libexecdir = ${exec_prefix}/libexec
bindir = ${exec_prefix}/bin
transform=s,x,x,
EXECSUFFIX=

PROG_BIN	= pagsh$(EXECSUFFIX) \
		  afslog$(EXECSUFFIX) \
		  kstring2key$(EXECSUFFIX)
PROG_LIBEXEC	= 
PROGS = $(PROG_BIN) $(PROG_LIBEXEC)

SOURCES = pagsh.c aklog.c kstring2key.c

OBJECTS = pagsh.o aklog.o kstring2key.o

all: $(PROGS)

Wall:
	make CFLAGS="-g -Wall -Wno-comment -Wmissing-prototypes -Wmissing-declarations -D__USE_FIXED_PROTOTYPES__"

.c.o:
	$(CC) -c $(DEFS) -I../../include -I$(srcdir) $(CFLAGS) $(CPPFLAGS) $<

install: all
	$(MKINSTALLDIRS) $(DESTDIR)$(bindir)
	for x in $(PROG_BIN); do \
	  $(INSTALL_PROGRAM) $$x $(DESTDIR)$(bindir)/`echo $$x | sed '$(transform)'`; \
	done

uninstall:
	for x in $(PROG_BIN); do \
	  rm -f $(DESTDIR)$(bindir)/`echo $$x | sed '$(transform)'`; \
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

pagsh$(EXECSUFFIX): pagsh.o
	$(LINK) $(LD_FLAGS) $(LDFLAGS) -o $@ pagsh.o $(LIBS)

afslog$(EXECSUFFIX): aklog.o
	$(LINK) $(LD_FLAGS) $(LDFLAGS) -o $@ aklog.o $(LIBS)

kstring2key$(EXECSUFFIX): kstring2key.o
	$(LINK) $(LD_FLAGS) $(LDFLAGS) -o $@ kstring2key.o $(LIBS)


$(OBJECTS): ../../include/config.h

.PHONY: all Wall install uninstall check clean mostlyclean distclean realclean
