# Generated automatically from Makefile.in by configure.
#
# $KTH: Makefile.in,v 1.25.2.1 2000/06/23 03:20:05 assar Exp $
#

SHELL = /bin/sh

srcdir = .

CC = gcc
LINK = $(CC)
AR = ar
LN_S = ln -s
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

LIB_DEPS = -L../../kafs -lkafs			\
	   -L../../krb -lkrb			\
	   -L../../des -ldes			\
	   -L../../roken -lroken		\
	   -lc
# LIB_DEPS =

PICFLAGS = -fpic
LDSHARED = ld -Bshareable
SHLIBEXT = so.1.0.8
LD_FLAGS = -Wl,-R$(libdir)
 
LIBNAME = afskauthlib
LIB = $(LIBNAME).$(SHLIBEXT)

SOURCES = verify.c

OBJECTS = verify.o

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
	$(LDSHARED) $(CFLAGS) -o $@ $(OBJECTS) $(LD_FLAGS) $(LIB_DEPS)

.PHONY: all Wall install uninstall check clean mostlyclean distclean realclean
