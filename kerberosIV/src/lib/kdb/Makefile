# Generated automatically from Makefile.in by configure.
#
# $KTH: Makefile.in,v 1.40.4.1 2000/06/23 03:20:00 assar Exp $
#

SHELL = /bin/sh

srcdir = .

CC = gcc
LINK = $(CC)
AR = ar
RANLIB = ranlib
LN_S = ln -s
DEFS = -DHAVE_CONFIG_H -DROKEN_RENAME
CFLAGS =  -g -O2 $(WFLAGS)
WFLAGS = 

INSTALL = /usr/bin/install -c
INSTALL_DATA	= ${INSTALL} -m 644
MKINSTALLDIRS = ../../mkinstalldirs

prefix = /usr/athena
exec_prefix = ${prefix}
libdir = ${exec_prefix}/lib

PICFLAGS = 
 
LIB_DBM  = 
LIB_DEPS =  $(LIB_DBM) -lc
build_symlink_command   = @true
install_symlink_command = @true

LIBNAME = $(LIBPREFIX)kdb
LIBEXT = a
SHLIBEXT = nobuild
LIBPREFIX = lib
LDSHARED = ld -Bshareable
LIB = $(LIBNAME).$(LIBEXT)

SOURCES = krb_cache.c krb_kdb_utils.c copykey.c krb_lib.c \
	krb_dbm.c print_princ.c

OBJECTS = krb_cache.o krb_kdb_utils.o copykey.o krb_lib.o \
	krb_dbm.o print_princ.o

all: $(LIB)

Wall:
		make CFLAGS="-g -Wall -Wno-comment -Wmissing-prototypes -Wmissing-declarations -D__USE_FIXED_PROTOTYPES__"

.c.o:
	$(CC) -c $(DEFS) -I../../include -I$(srcdir) -I. $(CFLAGS) $(CPPFLAGS) $(PICFLAGS) $<

install: all
	$(MKINSTALLDIRS) $(DESTDIR)$(libdir)
	$(INSTALL_DATA)  $(LIB) $(DESTDIR)$(libdir)/$(LIB)
	@true

uninstall:
	rm -f $(DESTDIR)$(libdir)/$(LIB)

TAGS: $(SOURCES)
	etags $(SOURCES)

check:

clean:
	rm -f $(LIB) *.o *.a *.so *.so.* so_locations

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
	$(LDSHARED) -o $@ $(OBJECTS) $(LIB_DEPS)
	@true

$(OBJECTS): ../../include/config.h roken_rename.h

roken_rename.h:
	$(LN_S) $(srcdir)/../krb/roken_rename.h .

.PHONY: all Wall install uninstall check clean mostlyclean distclean realclean
