# Generated automatically from Makefile.in by configure.
#
# $KTH: Makefile.in,v 1.29.4.1 2000/06/23 03:20:00 assar Exp $
#

SHELL = /bin/sh

srcdir = .

CC = gcc
LINK = $(CC)
AR = ar
LN_S = ln -s
RANLIB = ranlib
DEFS = -DHAVE_CONFIG_H -DROKEN_RENAME
CFLAGS =  -g -O2 $(WFLAGS)
WFLAGS = 

INSTALL = /usr/bin/install -c
INSTALL_DATA	= ${INSTALL} -m 644
MKINSTALLDIRS = ../../mkinstalldirs

prefix = /usr/athena
exec_prefix = ${prefix}
libdir = ${exec_prefix}/lib

PICFLAGS = # 

LIBNAME = $(LIBPREFIX)acl
#LIBEXT = a Always build archive library!
LIBEXT = a
LIBPREFIX = lib
SHLIBEXT = nobuild
LDSHARED = ld -Bshareable
LIB = $(LIBNAME).$(LIBEXT)

SOURCES = acl_files.c

OBJECTS = acl_files.o

all: $(LIB)

Wall:
	make CFLAGS="-g -Wall -Wno-comment -Wmissing-prototypes -Wmissing-declarations -D__USE_FIXED_PROTOTYPES__"

.c.o:
	$(CC) -c $(DEFS) -I../../include -I$(srcdir) -I. $(CFLAGS) $(PICFLAGS) $(CPPFLAGS) $<

install: all
	$(MKINSTALLDIRS) $(DESTDIR)$(libdir)
	$(INSTALL_DATA) $(LIB) $(DESTDIR)$(libdir)/$(LIB)

uninstall:
	rm -f $(DESTDIR)$(libdir)/$(LIB)

TAGS: $(SOURCES)
	etags $(SOURCES)

check:

clean:
	rm -f $(LIB) *.o *.a

mostlyclean: clean

distclean: clean
	rm -f Makefile *.tab.c *~ roken_rename.h

realclean: distclean
	rm -f TAGS

$(LIBNAME).a: $(OBJECTS)
	rm -f $@
	$(AR) cr $@ $(OBJECTS)
	-$(RANLIB) $@

$(LIBNAME).$(SHLIBEXT): $(OBJECTS)
	rm -f $@
	$(LDSHARED) -o $@ $(OBJECTS)

$(OBJECTS): ../../include/config.h roken_rename.h

roken_rename.h:
	$(LN_S) $(srcdir)/../krb/roken_rename.h .

.PHONY: all Wall install uninstall check clean mostlyclean distclean realclean
