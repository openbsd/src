# Generated automatically from Makefile.in by configure.
#
# $KTH: Makefile.in,v 1.47.4.1 2000/06/23 03:20:01 assar Exp $
#

SHELL = /bin/sh

srcdir = .

CC = gcc
LINK = $(CC)
AR = ar
RANLIB = ranlib
CP = cp
LN_S = ln -s
DEFS = -DHAVE_CONFIG_H -DROKEN_RENAME
CFLAGS =  -g -O2 $(WFLAGS)
WFLAGS = 

INSTALL = /usr/bin/install -c
INSTALL_DATA	= ${INSTALL} -m 644
MKINSTALLDIRS = ../../mkinstalldirs

COMPILE_ET = ../com_err/compile_et

prefix = /usr/athena
exec_prefix = ${prefix}
libdir = ${exec_prefix}/lib

top_builddir = ../..

includedir = ${prefix}/include

incdir = $(includedir)
inc_DATA = kadm_err.h
idir = $(top_builddir)/include

PICFLAGS = 

LIB_DEPS = -L../krb -lkrb \
	   -L../des -ldes \
	   -lc
# LIB_DEPS = 

build_symlink_command   = @true
install_symlink_command = @true

LIBNAME = $(LIBPREFIX)kadm
LIBEXT = a
LIBPREFIX = lib
EXECSUFFIX = 
SHLIBEXT = nobuild
LDSHARED = ld -Bshareable
LIB = $(LIBNAME).$(LIBEXT)

SOURCES = kadm_cli_wrap.c kadm_err.c kadm_stream.c kadm_supp.c check_password.c

OBJECTS = kadm_cli_wrap.o kadm_err.o kadm_stream.o kadm_supp.o check_password.o

all: $(LIB) all-local

Wall:
	make CFLAGS="-g -Wall -Wno-comment -Wmissing-prototypes -Wmissing-declarations -D__USE_FIXED_PROTOTYPES__"

.c.o:
	$(CC) -c $(DEFS) -I. -I../../include -I$(srcdir) $(CFLAGS) $(CPPFLAGS) $(PICFLAGS) $<

install: all
	$(MKINSTALLDIRS) $(DESTDIR)$(libdir)
	$(INSTALL_DATA)  $(LIB) $(DESTDIR)$(libdir)/$(LIB)
	@true
	$(MKINSTALLDIRS) $(DESTDIR)$(includedir)
	@for i in $(inc_DATA); do \
	echo "  $(INSTALL_DATA) $$i $(DESTDIR)$(incdir)/$$i";\
	$(INSTALL_DATA) $$i $(DESTDIR)$(incdir)/$$i; done

uninstall:
	rm -f $(DESTDIR)$(libdir)/$(LIB)
	@for i in $(inc_DATA); do \
	echo "  rm -f $(DESTDIR)$(incdir)/$$i";\
	rm -f $(DESTDIR)$(incdir)/$$i; done

TAGS: $(SOURCES)
	etags $(SOURCES)

check:

clean:
	rm -f $(LIB) *.o *.a *.so *.so.* so_locations kadm_err.c kadm_err.h

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

kadm_err.c kadm_err.h: $(srcdir)/kadm_err.et
	$(COMPILE_ET) $(srcdir)/kadm_err.et

$(OBJECTS): ../../include/config.h roken_rename.h
$(OBJECTS): kadm_err.h kadm_locl.h

roken_rename.h:
	$(LN_S) $(srcdir)/../krb/roken_rename.h .

all-local: $(inc_DATA)
	@for i in $(inc_DATA); do \
		if cmp -s  $$i $(idir)/$$i 2> /dev/null ; then :; else\
			echo " $(CP) $$i $(idir)/$$i"; \
			$(CP) $$i $(idir)/$$i; \
		fi ; \
	done

.PHONY: all Wall install uninstall check clean mostlyclean distclean realclean all-local
