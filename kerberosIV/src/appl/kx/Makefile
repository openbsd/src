# Generated automatically from Makefile.in by configure.
# $KTH: Makefile.in,v 1.39.18.1 2000/06/23 02:56:38 assar Exp $

SHELL = /bin/sh

srcdir = .

XINCS =  -I/usr/X11R6/include
XLIBS =  -L/usr/X11R6/lib -lXau  -lSM -lICE 

CC = gcc
LINK = $(CC)
AR = ar
DEFS = -DHAVE_CONFIG_H
CFLAGS =  -g -O2 $(WFLAGS)
WFLAGS =  
CPPFLAGS= $(XINCS) 
LD_FLAGS = 
INSTALL = /usr/bin/install -c
INSTALL_PROGRAM = ${INSTALL}
INSTALL_SCRIPT  = ${INSTALL_PROGRAM}
LN_S = ln -s
LIBS = $(XLIBS) -lutil  
MKINSTALLDIRS = ../../mkinstalldirs

VERSION = 1.0.8
PACKAGE = krb4

prefix = /usr/athena
exec_prefix = ${prefix}
libdir = ${exec_prefix}/lib
libexecdir = ${exec_prefix}/libexec
bindir = ${exec_prefix}/bin
transform=s,x,x,
EXECSUFFIX=

X_PROGS_BIN     = kx$(EXECSUFFIX)
X_SCRIPTS_BIN   = rxterm rxtelnet tenletxr
X_PROGS_LIBEXEC = kxd$(EXECSUFFIX)

PROG_BIN	= $(X_PROGS_BIN)
SCRIPTS_BIN	= $(X_SCRIPTS_BIN)
PROG_LIBEXEC	= $(X_PROGS_LIBEXEC)
PROGS = $(PROG_BIN) $(SCRIPTS_BIN) $(PROG_LIBEXEC)

WRITEAUTH = # writeauth.o

SOURCES_KX     = kx.c writeauth.c
SOURCES_KXD    = kxd.c writeauth.c
SOURCES_COMMON = common.c krb4.c krb5.c context.c

OBJECTS_KX     = kx.o common.o krb4.o krb5.o context.o $(WRITEAUTH)
OBJECTS_KXD    = kxd.o common.o krb4.o krb5.o context.o $(WRITEAUTH)

OBJECTS = $(OBJECTS_KX) $(OBJECTS_KXD)
SOURCES = $(SOURCES_KX) $(SOURCES_KXD) $(SOURCES_COMMON)

all: $(PROGS)

Wall:
	make CFLAGS="-g -Wall -Wno-comment -Wmissing-prototypes -Wmissing-declarations -D__USE_FIXED_PROTOTYPES__"

.c.o:
	$(CC) -c $(DEFS) -I../../include -I$(srcdir) -I$(srcdir)/../kauth $(CFLAGS) $(CPPFLAGS) $<

install: all
	$(MKINSTALLDIRS) $(DESTDIR)$(bindir) $(DESTDIR)$(libexecdir)
	PROG_BIN='$(PROG_BIN)'; for x in $$PROG_BIN; do \
	  $(INSTALL_PROGRAM) $$x $(DESTDIR)$(bindir)/`echo $$x | sed '$(transform)'`; \
	done
	SCRIPTS_BIN='$(SCRIPTS_BIN)'; for x in $$SCRIPTS_BIN; do \
	  $(INSTALL_SCRIPT) $$x $(DESTDIR)$(bindir)/`echo $$x | sed '$(transform)'`; \
	done
	PROG_LIBEXEC='$(PROG_LIBEXEC)'; for x in $$PROG_LIBEXEC; do \
	  $(INSTALL_PROGRAM) $$x $(DESTDIR)$(libexecdir)/`echo $$x | sed '$(transform)'`; \
	done

uninstall:
	PROG_BIN='$(PROG_BIN)'; for x in $$PROG_BIN; do \
	  rm -f $(DESTDIR)$(bindir)/`echo $$x | sed '$(transform)'`; \
	done
	SCRIPTS_BIN='$(SCRIPTS_BIN)'; for x in $$SCRIPTS_BIN; do \
	  rm -f $(DESTDIR)$(bindir)/`echo $$x | sed '$(transform)'`; \
	done
	PROG_LIBEXEC='$(PROG_LIBEXEC)'; for x in $$PROG_LIBEXEC; do \
	  rm -f $(DESTDIR)$(libexecdir)/`echo $$x | sed '$(transform)'`; \
	done

TAGS: $(SOURCES)
	etags $(SOURCES)

check:

clean:
	rm -f *.a *.o $(PROGS) encdata.c

mostlyclean: clean

distclean: clean
	rm -f Makefile *.tab.c *~

realclean: distclean
	rm -f TAGS

KLIB= -L../../lib/krb -lkrb -L../../lib/des -ldes
LIBROKEN=-L../../lib/roken -lroken

encdata.c:
	$(LN_S) $(srcdir)/../kauth/encdata.c .

kx$(EXECSUFFIX): $(OBJECTS_KX)
	$(LINK) $(LD_FLAGS) $(LDFLAGS) -o $@ $(OBJECTS_KX) $(KLIB) $(LIBROKEN) $(LIBS) $(LIBROKEN)

kxd$(EXECSUFFIX): $(OBJECTS_KXD)
	$(LINK) $(LD_FLAGS) $(LDFLAGS) -o $@ $(OBJECTS_KXD) $(KLIB) $(LIBROKEN) $(LIBS) $(LIBROKEN)

rxterm: rxterm.in
	sed -e "s!%bindir%!$(bindir)!" -e "s!%VERSION%!$(VERSION)!" -e "s!%PACKAGE%!$(PACKAGE)!" $(srcdir)/rxterm.in > $@
	chmod +x $@

rxtelnet: rxtelnet.in
	sed -e "s!%bindir%!$(bindir)!" -e "s!%VERSION%!$(VERSION)!" -e "s!%PACKAGE%!$(PACKAGE)!" $(srcdir)/rxtelnet.in > $@
	chmod +x $@

tenletxr: tenletxr.in
	sed -e "s!%bindir%!$(bindir)!" -e "s!%VERSION%!$(VERSION)!" -e "s!%PACKAGE%!$(PACKAGE)!" $(srcdir)/tenletxr.in > $@
	chmod +x $@

$(OBJECTS): ../../include/config.h

.PHONY: all Wall install uninstall check clean mostlyclean distclean realclean
