# Generated automatically from Makefile.in by configure.
# 
# $KTH: Makefile.in,v 1.32 1999/03/11 13:58:09 joda Exp $
#

SHELL		= /bin/sh

srcdir		= .
top_srcdir	= ../../..

top_builddir		= ../../..

CC 	= gcc
RANLIB 	= ranlib
DEFS 	= -DHAVE_CONFIG_H
CFLAGS 	=  -g -O2 $(WFLAGS)
WFLAGS	= 
CPPFLAGS=  -I. -I$(srcdir) -I$(top_builddir) -I$(top_srcdir) -I$(top_builddir)/include -I$(top_srcdir)/include -I$(srcdir)/../common  
LD_FLAGS = 
LIB_tgetent = -ltermcap
LIBS	 = -lutil   -lreadline $(LIB_tgetent)
MKINSTALLDIRS = $(top_srcdir)/mkinstalldirs

INSTALL = /usr/bin/install -c
INSTALL_PROGRAM = ${INSTALL}

prefix 	= /usr/athena
exec_prefix = ${prefix}
bindir = ${exec_prefix}/bin
libdir = ${exec_prefix}/lib
transform=s,x,x,
EXECSUFFIX=

INCTOP = $(top_builddir)/include

LIBTOP = $(top_builddir)/lib

PROGS = ftp$(EXECSUFFIX)

ftp_SOURCES =		\
	cmds.c		\
	cmdtab.c	\
	domacro.c	\
	ftp.c		\
	globals.c	\
	kauth.c		\
	krb4.c		\
	main.c		\
	ruserpass.c	\
	security.c

ftp_OBJS =		\
	cmds.o		\
	cmdtab.o	\
	domacro.o	\
	ftp.o		\
	globals.o	\
	kauth.o		\
	krb4.o		\
	main.o		\
	ruserpass.o	\
	security.o

OBJECTS = $(ftp_OBJS)
SOURCES = $(ftp_SOURCES)

all: $(PROGS)

.c.o:
	$(CC) -c -I$(srcdir) -I../../../include $(DEFS) $(CFLAGS) $(CPPFLAGS) $<

install: all
	$(MKINSTALLDIRS) $(DESTDIR)$(bindir)
	for x in $(PROGS); do \
	  $(INSTALL_PROGRAM) $$x $(DESTDIR)$(bindir)/`echo $$x | sed '$(transform)'`; \
	done

uninstall:
	for x in $(PROGS); do \
	  rm -f $(DESTDIR)$(bindir)/`echo $$x | sed '$(transform)'`; \
	done

ftp$(EXECSUFFIX): $(ftp_OBJS)
	$(CC) $(LD_FLAGS) $(LDFLAGS) -o $@ $(ftp_OBJS) -L../common -lcommon -L$(LIBTOP)/krb -lkrb -L$(LIBTOP)/des -ldes -L$(LIBTOP)/roken -lroken $(LIBS) -L$(LIBTOP)/roken -lroken

TAGS:	$(SOURCES)
	etags $(SOURCES)

clean:
	rm -f *~ *.o core ftp$(EXECSUFFIX) \#*

mostlyclean: clean

distclean: clean
	rm -f Makefile

realclean: distclean
	rm -f TAGS

$(OBJECTS): ../../../include/config.h

.PHONY: all install uninstall clean cleandir distclean
