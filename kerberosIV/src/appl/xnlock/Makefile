# Generated automatically from Makefile.in by configure.
# $KTH: Makefile.in,v 1.41.16.1 2001/03/03 22:12:26 assar Exp $

SHELL = /bin/sh

srcdir = .

top_builddir = ../..

XINCS =  -I/usr/X11R6/include
XLIBS =  -L/usr/X11R6/lib -lXt  -lSM -lICE -lX11 

CC = gcc
LINK = $(CC)
RANLIB = ranlib
DEFS = -DHAVE_CONFIG_H -DBINDIR='"$(bindir)"'
CFLAGS = $(XINCS)  -g -O2 $(WFLAGS)
WFLAGS =  
LD_FLAGS = 
LIBS = $(XLIBS) -lutil  
INSTALL = /usr/bin/install -c
INSTALL_DATA = ${INSTALL} -m 644
INSTALL_PROGRAM = ${INSTALL}
MKINSTALLDIRS = ../../mkinstalldirs

prefix = /usr/athena
exec_prefix = ${prefix}
libdir = ${exec_prefix}/lib
bindir = ${exec_prefix}/bin
mandir = ${prefix}/man
transform=s,x,x,
EXECSUFFIX=

X_PROGS_BIN = xnlock$(EXECSUFFIX)

PROG_BIN	= $(X_PROGS_BIN)
PROG_LIBEXEC	= 
PROGS = $(PROG_BIN) $(PROG_LIBEXEC)

MAN1 = xnlock.man

SOURCES = xnlock.c

OBJECTS = xnlock.o

all: $(PROGS)

Wall:
	make CFLAGS="$(XINCS) -g -Wall -Wno-comment -Wmissing-prototypes -Wmissing-declarations -D__USE_FIXED_PROTOTYPES__"

.c.o:
	$(CC) -c $(DEFS) -I../../include -I$(srcdir) $(CFLAGS) $(CPPFLAGS) $<

install: all
	$(MKINSTALLDIRS) $(DESTDIR)$(bindir)
	PROG_BIN='$(PROG_BIN)'; for x in $$PROG_BIN; do \
	  $(INSTALL_PROGRAM) $$x $(DESTDIR)$(bindir)/`echo $$x | sed '$(transform)'`; \
	done
	$(MKINSTALLDIRS) $(DESTDIR)$(mandir)/man1
	for x in $(MAN1); do \
	  b=`echo $$x | sed -e 's/\.man//' -e '$(transform)'`.1; \
	  $(INSTALL_DATA) $(srcdir)/$$x $(DESTDIR)$(mandir)/man1/$$b; \
	done
uninstall:
	PROG_BIN='$(PROG_BIN)'; for x in $$PROG_BIN; do \
	  rm -f $(DESTDIR)$(bindir)/`echo $$x | sed '$(transform)'`; \
	done
	for x in $(MAN1); do \
	  b=`echo $$x | sed -e 's/\.man//' -e '$(transform)'`.1; \
	  rm -f $(DESTDIR)$(mandir)/man1/$$b; \
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

KLIB=-L../../lib/krb -lkrb -L../../lib/des -ldes
LIBROKEN=-L../../lib/roken -lroken

xnlock$(EXECSUFFIX): xnlock.o
	$(LINK) $(LD_FLAGS) $(LDFLAGS) -o $@ xnlock.o -L$(top_builddir)/lib/kafs -lkafs  $(KLIB) $(LIBROKEN) $(LIBS) $(LIBROKEN)

$(OBJECTS): ../../include/config.h

.PHONY: all Wall install uninstall check clean mostlyclean distclean realclean
