# Generated automatically from Makefile.in by configure.
#
# $KTH: Makefile.in,v 1.50.2.2 2000/12/07 16:44:12 assar Exp $
#

SHELL = /bin/sh

srcdir = .

CC = gcc
LINK = $(CC)
AR = ar
RANLIB = ranlib
LN_S = ln -s
DEFS = -DHAVE_CONFIG_H -DROKEN_RENAME -DLIBDIR='"$(libdir)"' 
CFLAGS =  -g -O2 $(WFLAGS)
WFLAGS = 

INSTALL = /usr/bin/install -c
INSTALL_DATA	= ${INSTALL} -m 644
MKINSTALLDIRS = ../../mkinstalldirs

prefix = /usr/athena
exec_prefix = ${prefix}
libdir = ${exec_prefix}/lib

PICFLAGS = 
 
LIB_DEPS =  -lc
build_symlink_command   = @true
install_symlink_command = @true

LIBNAME = $(LIBPREFIX)kafs
LIBEXT = a
SHLIBEXT = nobuild
LIBPREFIX = lib
LDSHARED = ld -Bshareable
AFS_EXTRA_OBJS	= 
AFS_EXTRA_LIBS	= 
LIB = $(LIBNAME).$(LIBEXT) $(AFS_EXTRA_LIBS)

SOURCES = afssys.c afskrb.c common.c afslib.c

EXTRA_SOURCE = issuid.c strlcpy.c strlcat.c

EXTRA_OBJECT = issuid.o strlcpy.o strlcat.o

OBJECTS = afssys.o afskrb.o common.o $(EXTRA_OBJECT) $(AFS_EXTRA_OBJS)

all: $(LIB)

Wall:
	make CFLAGS="-g -Wall -Wno-comment -Wmissing-prototypes -Wmissing-declarations -D__USE_FIXED_PROTOTYPES__"

.c.o:
	$(CC) -c $(DEFS) -I../../include -I$(srcdir) -I. $(CFLAGS) $(CPPFLAGS) $(PICFLAGS) $<

install: all
	$(MKINSTALLDIRS) $(DESTDIR)$(libdir)
	@for i in $(LIB); do \
	echo "$(INSTALL)  $$i $(DESTDIR)$(libdir)/$$i" ;\
	$(INSTALL)  $$i $(DESTDIR)$(libdir)/$$i ; done
	@true

uninstall:
	@for i in $(LIB); do \
	echo "rm -f $(DESTDIR)$(libdir)/$$i" ;\
	rm -f $(DESTDIR)$(libdir)/$$i ; done

TAGS: $(SOURCES)
	etags $(SOURCES)

check:

clean:
	rm -f $(LIB) *.o *.a *.so *.so.* so_locations $(EXTRA_SOURCE)

mostlyclean: clean

distclean: clean
	rm -f Makefile *.tab.c *~ roken_rename.h

realclean: distclean
	rm -f TAGS

$(LIBNAME)_pic.a: $(OBJECTS)
	rm -f $@
	$(AR) cr $@ $(OBJECTS)
	-$(RANLIB) $@

$(LIBNAME).a: $(OBJECTS)
	rm -f $@
	$(AR) cr $@ $(OBJECTS)
	-$(RANLIB) $@

$(LIBNAME).$(SHLIBEXT): $(OBJECTS) $(LIBNAME)_pic.a
	rm -f $@
	$(LDSHARED) -o $@ $(OBJECTS) $(LIB_DEPS)
	@true

# AIX: this almost works with gcc, but somehow it fails to use the
# correct ld, use ld instead
afslib.so: afslib.o
	ld -o $@ -bM:SRE -bI:$(srcdir)/afsl.exp -bE:$(srcdir)/afslib.exp  afslib.o -lc

$(OBJECTS): ../../include/config.h roken_rename.h

roken_rename.h:
	$(LN_S) $(srcdir)/../krb/roken_rename.h .

.PHONY: all Wall install uninstall check clean mostlyclean distclean realclean

issuid.c:
	$(LN_S) $(srcdir)/../roken/issuid.c .

strlcat.c:
	$(LN_S) $(srcdir)/../roken/strlcat.c .

strlcpy.c:
	$(LN_S) $(srcdir)/../roken/strlcpy.c .

