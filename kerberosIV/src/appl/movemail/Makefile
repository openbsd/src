# Generated automatically from Makefile.in by configure.
# $KTH: Makefile.in,v 1.28 1999/04/07 18:39:40 assar Exp $

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
LIBS =  -lutil  
MKINSTALLDIRS = ../../mkinstalldirs

prefix = /usr/athena
exec_prefix = ${prefix}
libdir = ${exec_prefix}/lib
bindir = ${exec_prefix}/bin
libexecdir = ${exec_prefix}/libexec
transform=s,x,x,
EXECSUFFIX=

PROG_BIN	= 
PROG_LIBEXEC	= movemail$(EXECSUFFIX)
PROGS = $(PROG_BIN) $(PROG_LIBEXEC)

SOURCES = movemail.c pop.c

OBJECTS = movemail.o pop.o

all: $(PROGS)

Wall:
	make CFLAGS="-g -Wall -Wno-comment -Wmissing-prototypes -Wmissing-declarations -D__USE_FIXED_PROTOTYPES__"

.c.o:
	$(CC) -c $(DEFS) -I../../include -I$(srcdir) $(CFLAGS) $(CPPFLAGS) $<

install: all
	$(MKINSTALLDIRS) $(DESTDIR)$(bindir)
	for x in $(PROG_LIBEXEC); do \
	  $(INSTALL_PROGRAM) $$x $(DESTDIR)$(libexecdir)/`echo $$x | sed '$(transform)'`; \
	done

uninstall:
	for x in $(PROG_LIBEXEC); do \
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

KLIB=-L../../lib/krb -lkrb -L../../lib/des -ldes
LIBROKEN=-L../../lib/roken -lroken

movemail$(EXECSUFFIX): movemail.o pop.o
	$(LINK) $(LD_FLAGS) $(LDFLAGS) -o $@ movemail.o pop.o $(KLIB) $(LIBROKEN) $(LIBS) $(LIBROKEN)

$(OBJECTS): ../../include/config.h

.PHONY: all Wall install uninstall check clean mostlyclean distclean realclean
