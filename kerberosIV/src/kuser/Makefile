# Generated automatically from Makefile.in by configure.
# $KTH: Makefile.in,v 1.30 1999/03/10 19:01:14 joda Exp $

SHELL	= /bin/sh

srcdir	= .

top_builddir	= ..

CC		= gcc
LINK		= $(CC)
AR		= ar
RANLIB		= ranlib
DEFS		= -DHAVE_CONFIG_H
CFLAGS		=  -g -O2 $(WFLAGS)
WFLAGS		= 
LD_FLAGS	= 
INSTALL		= /usr/bin/install -c
INSTALL_PROGRAM = ${INSTALL}
LIBS		= -lutil  
KRB_KAFS_LIB	= -L$(top_builddir)/lib/kafs -lkafs 
MKINSTALLDIRS	= ../mkinstalldirs

prefix = /usr/athena
exec_prefix = ${prefix}
bindir = ${exec_prefix}/bin
libdir = ${exec_prefix}/lib
transform=s,x,x,
EXECSUFFIX=

PROGS = kinit$(EXECSUFFIX) \
	kdestroy$(EXECSUFFIX) \
	klist$(EXECSUFFIX)

SOURCES = kinit.c kdestroy.c klist.c

OBJECTS = kinit.o kdestroy.o klist.o

all: $(PROGS)

Wall:
	make CFLAGS="-g -Wall -Wno-comment -Wmissing-prototypes -Wmissing-declarations -D__USE_FIXED_PROTOTYPES__"

.c.o:
	$(CC) -c $(DEFS) -I../include -I$(srcdir) $(CFLAGS) $(CPPFLAGS) $<

install: all
	$(MKINSTALLDIRS) $(DESTDIR)$(bindir)
	for x in $(PROGS); do \
	  $(INSTALL_PROGRAM) $$x $(DESTDIR)$(bindir)/`echo $$x | sed '$(transform)'`; \
	done

uninstall:
	for x in $(PROGS); do \
	  rm -f $(DESTDIR)$(bindir)/`echo $$x | sed '$(transform)'`; \
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

KLIB=-L../lib/krb -lkrb -L../lib/des -ldes
LIBROKEN=-L../lib/roken -lroken

kinit$(EXECSUFFIX): kinit.o
	$(LINK) $(LD_FLAGS) $(LDFLAGS) -o $@ kinit.o $(KLIB) $(LIBROKEN) $(LIBS) $(LIBROKEN)

kdestroy$(EXECSUFFIX): kdestroy.o
	$(LINK) $(LD_FLAGS) $(LDFLAGS) -o $@ kdestroy.o $(KRB_KAFS_LIB) $(KLIB) $(LIBROKEN) $(LIBS) $(LIBROKEN)

klist$(EXECSUFFIX): klist.o
	$(LINK) $(LD_FLAGS) $(LDFLAGS) -o $@ klist.o $(KRB_KAFS_LIB) $(KLIB) $(LIBROKEN) $(LIBS) $(LIBROKEN)

# su move to appl/bsd

$(OBJECTS): ../include/config.h

.PHONY: all Wall install uninstall check clean mostlyclean distclean realclean
