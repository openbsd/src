# Generated automatically from Makefile.in by configure.
# $KTH: Makefile.in,v 1.32 1999/03/10 19:01:10 joda Exp $

SHELL = /bin/sh

srcdir = .

CC = gcc
LINK = $(CC)
AR = ar
RANLIB = ranlib
DEFS = -DHAVE_CONFIG_H
CFLAGS =  -g -O2 $(WFLAGS)
WFLAGS = 
LD_FLAGS = 
LIBS = -lutil  
LIB_DBM = 

INSTALL = /usr/bin/install -c
INSTALL_PROGRAM = ${INSTALL}

MKINSTALLDIRS = ../mkinstalldirs

prefix = /usr/athena
exec_prefix = ${prefix}
libdir = ${exec_prefix}/lib
sbindir = ${exec_prefix}/sbin
transform=s,x,x,
EXECSUFFIX=

PROGS = ext_srvtab$(EXECSUFFIX) \
	kdb_destroy$(EXECSUFFIX) \
	kdb_edit$(EXECSUFFIX) \
	kdb_init$(EXECSUFFIX) \
	kdb_util$(EXECSUFFIX) \
	kstash$(EXECSUFFIX)

SOURCES = ext_srvtab.c kdb_destroy.c kdb_edit.c \
          kdb_init.c kdb_util.c kstash.c

OBJECTS = ext_srvtab.o kdb_destroy.o kdb_edit.o \
          kdb_init.o kdb_util.o kstash.o

all: $(PROGS)

Wall:
	make CFLAGS="-g -Wall -Wno-comment -Wmissing-prototypes -Wmissing-declarations -D__USE_FIXED_PROTOTYPES__"

.c.o:
	$(CC) -c $(DEFS) -I../include -I$(srcdir) $(CFLAGS) $(CPPFLAGS) $<

install: all
	$(MKINSTALLDIRS) $(DESTDIR)$(sbindir)
	for x in $(PROGS); do \
	  $(INSTALL_PROGRAM) $$x $(DESTDIR)$(sbindir)/`echo $$x|sed '$(transform)'`; \
	done

uninstall:
	for x in $(PROGS); do \
	  rm -f $(DESTDIR)$(sbindir)/`echo $$x|sed '$(transform)'`; \
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

KLIB=-L../lib/kdb -lkdb -L../lib/krb -lkrb -L../lib/des -ldes
LIBROKEN= -L../lib/roken -lroken

ext_srvtab$(EXECSUFFIX): ext_srvtab.o
	$(LINK) $(LD_FLAGS) $(LDFLAGS) -o $@ ext_srvtab.o $(KLIB) $(LIBROKEN) $(LIB_DBM) $(LIBS) $(LIBROKEN)

kdb_destroy$(EXECSUFFIX): kdb_destroy.o
	$(LINK) $(LD_FLAGS) $(LDFLAGS) -o $@ kdb_destroy.o $(KLIB) $(LIBROKEN) $(LIB_DBM) $(LIBS) $(LIBROKEN)

kdb_edit$(EXECSUFFIX): kdb_edit.o
	$(LINK) $(LD_FLAGS) $(LDFLAGS) -o $@ kdb_edit.o $(KLIB) $(LIBROKEN) $(LIB_DBM) $(LIBS) $(LIBROKEN)

kdb_init$(EXECSUFFIX): kdb_init.o
	$(LINK) $(LD_FLAGS) $(LDFLAGS) -o $@ kdb_init.o $(KLIB) $(LIBROKEN) $(LIB_DBM) $(LIBS) $(LIBROKEN)

kdb_util$(EXECSUFFIX): kdb_util.o
	$(LINK) $(LD_FLAGS) $(LDFLAGS) -o $@ kdb_util.o $(KLIB) $(LIBROKEN) $(LIB_DBM) $(LIBS) $(LIBROKEN)

kstash$(EXECSUFFIX): kstash.o
	$(LINK) $(LD_FLAGS) $(LDFLAGS) -o $@ kstash.o $(KLIB) $(LIBROKEN) $(LIB_DBM) $(LIBS) $(LIBROKEN)

$(OBJECTS): ../include/config.h

.PHONY: all Wall install uninstall check clean mostlyclean distclean realclean
