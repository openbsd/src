# Generated automatically from Makefile.in by configure.
#
# $KTH: Makefile.in,v 1.31.16.1 2000/06/23 03:20:03 assar Exp $
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

PICFLAGS = 
 
LIBNAME = $(LIBPREFIX)editline
LIBEXT = a
SHLIBEXT = nobuild
LIBPREFIX = lib
LDSHARED = ld -Bshareable
LIB = $(LIBNAME).$(LIBEXT)
LIB2 = $(LIBPREFIX)el_compat.a
LIBS = $(LIB) #  $(LIB2)

LIB_DEPS =  -ltermcap -lc
build_symlink_command   = @true
install_symlink_command = @true
install_symlink_command2 = @true

SOURCES = edit_compat.c editline.c complete.c sysunix.c 

EXTRA_SOURCES = strdup.c snprintf.c
EXTRA_OBJECTS = strdup.o snprintf.o

libeditline_a_OBJECTS = editline.o complete.o sysunix.o $(EXTRA_OBJECTS)

libel_compat_a_OBJECTS = edit_compat.o

OBJECTS = $(libeditline_a_OBJECTS) $(libel_compat_a_OBJECTS)

all: $(LIBS)

Wall:
	make CFLAGS="-g -Wall -Wmissing-prototypes -Wmissing-declarations -D__USE_FIXED_PROTOTYPES__"

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
	rm -f $(LIBS) *.o *.a *.so *.so.* so_locations

mostlyclean: clean

distclean: clean
	rm -f Makefile *.tab.c *~

realclean: distclean
	rm -f TAGS

$(LIBNAME).a: $(libeditline_a_OBJECTS)
	rm -f $@
	$(AR) cr $@ $(libeditline_a_OBJECTS)
	-$(RANLIB) $@


$(LIBNAME).$(SHLIBEXT): $(libeditline_a_OBJECTS)
	rm -f $@
	$(LDSHARED) -o $@ $(libeditline_a_OBJECTS) $(LIB_DEPS)
	@true

$(LIB2): $(libel_compat_a_OBJECTS)
	rm -f $@
	$(AR) cr $@ $(libel_compat_a_OBJECTS)
	-$(RANLIB) $@

$(OBJECTS): ../../include/config.h

strdup.c:
	$(LN_S) $(srcdir)/../roken/strdup.c .
snprintf.c:
	$(LN_S) $(srcdir)/../roken/snprintf.c .

.PHONY: all Wall install uninstall check clean mostlyclean distclean realclean
