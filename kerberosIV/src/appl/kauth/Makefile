# Generated automatically from Makefile.in by configure.
# $KTH: Makefile.in,v 1.40.16.1 2000/06/23 02:52:31 assar Exp $

SHELL = /bin/sh

srcdir = .

top_builddir = ../..

CC = gcc
LINK = $(CC)
AR = ar
RANLIB = ranlib
DEFS = -DHAVE_CONFIG_H -DBINDIR='"$(bindir)"'
CFLAGS =  -g -O2 $(WFLAGS)
WFLAGS = 
LD_FLAGS = 
INSTALL = /usr/bin/install -c
INSTALL_PROGRAM = ${INSTALL}
INSTALL_SCRIPT  = ${INSTALL_PROGRAM}
LIBS = -lutil  
MKINSTALLDIRS = ../../mkinstalldirs

prefix = /usr/athena
exec_prefix = ${prefix}
libdir = ${exec_prefix}/lib
libexecdir = ${exec_prefix}/libexec
bindir = ${exec_prefix}/bin
transform=s,x,x,
EXECSUFFIX=

PROG_BIN	= kauth$(EXECSUFFIX)
SCRIPT_BIN	= ksrvtgt
PROG_LIBEXEC	= kauthd$(EXECSUFFIX)
PROGS = $(PROG_BIN) $(SCRIPT_BIN) $(PROG_LIBEXEC)

SOURCES_KAUTH  = kauth.c rkinit.c
SOURCES_KAUTHD = kauthd.c
SOURCES_COMMON = encdata.c marshall.c

OBJECTS_KAUTH  = kauth.o rkinit.o
OBJECTS_KAUTHD = kauthd.o
OBJECTS_COMMON = marshall.o encdata.o

OBJECTS = $(OBJECTS_KAUTH) $(OBJECTS_KAUTHD)
SOURCES = $(SOURCES_KAUTH) $(SOURCES_KAUTHD) $(SOURCES_COMMON)

KRB_KAFS_LIB = -L$(top_builddir)/lib/kafs -lkafs 

all: $(PROGS)

Wall:
	make CFLAGS="-g -Wall -Wno-comment -Wmissing-prototypes -Wmissing-declarations -D__USE_FIXED_PROTOTYPES__"

.c.o:
	$(CC) -c $(DEFS) -I../../include -I$(srcdir) $(CFLAGS) $(CPPFLAGS) $<

install: all
	$(MKINSTALLDIRS) $(DESTDIR)$(bindir) $(DESTDIR)$(libexecdir)
	for x in $(PROG_BIN); do \
	  $(INSTALL_PROGRAM) $$x $(DESTDIR)$(bindir)/`echo $$x| sed '$(transform)'`; \
	done
	for x in $(SCRIPT_BIN); do \
	  $(INSTALL_SCRIPT) $$x $(DESTDIR)$(bindir)/`echo $$x| sed '$(transform)'`; \
	done
	if test -f $(DESTDIR)$(bindir)/zrefresh -o -r  $(DESTDIR)$(bindir)/zrefresh; then \
	  true; \
	else \
	  $(INSTALL_PROGRAM) $(srcdir)/zrefresh $(DESTDIR)$(bindir)/`echo zrefresh | sed '$(transform)'`; \
	fi
	for x in $(PROG_LIBEXEC); do \
	  $(INSTALL_PROGRAM) $$x $(DESTDIR)$(libexecdir)/`echo $$x| sed '$(transform)'`; \
	done

uninstall:
	for x in $(PROG_BIN) $(SCRIPT_BIN); do \
	  rm -f $(DESTDIR)$(bindir)/`echo $$x| sed '$(transform)'`; \
	done
	for x in $(PROG_LIBEXEC); do \
	  rm -f $(DESTDIR)$(libexecdir)/`echo $$x| sed '$(transform)'`; \
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

kauth$(EXECSUFFIX): $(OBJECTS_KAUTH) $(OBJECTS_COMMON)
	$(LINK) $(LD_FLAGS) $(LDFLAGS) -o $@ $(OBJECTS_KAUTH) $(OBJECTS_COMMON) $(KRB_KAFS_LIB) $(KLIB) $(LIBROKEN) $(LIBS) $(LIBROKEN)

kauthd$(EXECSUFFIX): $(OBJECTS_KAUTHD) $(OBJECTS_COMMON)
	$(LINK) $(LD_FLAGS) $(LDFLAGS) -o $@ $(OBJECTS_KAUTHD) $(OBJECTS_COMMON) $(KLIB) $(LIBROKEN) $(LIBS) $(LIBROKEN)

ksrvtgt: ksrvtgt.in
	sed -e "s!%bindir%!$(bindir)!" $(srcdir)/ksrvtgt.in > $@
	chmod +x $@


$(OBJECTS): ../../include/config.h

.PHONY: all Wall install uninstall check clean mostlyclean distclean realclean
