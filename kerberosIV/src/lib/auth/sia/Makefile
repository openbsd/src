# Generated automatically from Makefile.in by configure.
#
# $KTH: Makefile.in,v 1.30.2.1 2000/06/23 03:20:06 assar Exp $
#

SHELL = /bin/sh

srcdir = .

CC = gcc
LINK = $(CC)
AR = ar
RANLIB = ranlib
DEFS = -DHAVE_CONFIG_H
CFLAGS =  -g -O2 $(WFLAGS)
WFLAGS = 

INSTALL = /usr/bin/install -c
INSTALL_DATA	= ${INSTALL} -m 644
MKINSTALLDIRS = ../../../mkinstalldirs

prefix = /usr/athena
exec_prefix = ${prefix}
libdir = ${exec_prefix}/lib

PICFLAGS = -fpic
SHARED = @SHARED@
LDSHARED = ld -Bshareable
SHLIBEXT = so.1.0.8
LD_FLAGS = -Wl,-R$(libdir)
 
LIB_DEPS = -L../../kafs -lkafs		\
	   -L../../kadm -lkadm			\
	   -L../../krb -lkrb			\
	   -L../../des -ldes			\
	   -L../../com_err -lcom_err		\
	   -L../../roken -lroken		\
	   			\
	   -lc
# LIB_DEPS =

LIBNAME = libsia_krb4
LIB = $(LIBNAME).$(SHLIBEXT)

SOURCES = sia.c posix_getpw.c

OBJECTS = sia.o posix_getpw.o

all: $(LIB)

Wall:
	make CFLAGS="-g -Wall -Wno-comment -Wmissing-prototypes -Wmissing-declarations -D__USE_FIXED_PROTOTYPES__"

.c.o:
	$(CC) -c $(DEFS) -I../../../include -I$(srcdir) $(CFLAGS) $(CPPFLAGS) $(PICFLAGS) $<

install: all
	$(MKINSTALLDIRS) $(DESTDIR)$(libdir)
	-if test "$(LIB)" != ""; then \
	  $(INSTALL_DATA) $(LIB) $(DESTDIR)$(libdir)/$(LIB) ; \
	fi

uninstall:
	-if test "$(LIB)" != ""; then \
	  rm -f $(DESTDIR)$(libdir)/$(LIB) ; \
	fi

TAGS: $(SOURCES)
	etags $(SOURCES)

check:

clean:
	rm -f $(LIB) *.o

mostlyclean: clean

distclean: clean
	rm -f Makefile *.tab.c *~

realclean: distclean
	rm -f TAGS

$(OBJECTS): ../../../include/config.h

$(LIB): $(OBJECTS)
	rm -f $@
	$(LDSHARED) -shared -o $@ -rpath $(libdir) -hidden -exported_symbol siad_\* $(OBJECTS) $(LIB_DEPS)

.PHONY: all Wall install uninstall check clean mostlyclean distclean realclean
