# Generated automatically from Makefile.in by configure.
# 
# $KTH: Makefile.in,v 1.41 1999/10/03 16:39:27 joda Exp $
#

srcdir		= .
top_srcdir	= ../../..

top_builddir	= ../../..

SHELL		= /bin/sh

CC 	= gcc
YACC	= yacc
RANLIB 	= ranlib
DEFS 	= -DHAVE_CONFIG_H
WFLAGS = 
CFLAGS 	=  -g -O2 $(WFLAGS)
LD_FLAGS = 
LIBS	= -lutil  
LIB_DBM = 
MKINSTALLDIRS = $(top_srcdir)/mkinstalldirs

INSTALL = /usr/bin/install -c
INSTALL_PROGRAM = ${INSTALL}

LN_S = ln -s

prefix 	= /usr/athena
exec_prefix = ${prefix}
libdir = ${exec_prefix}/lib
libexecdir = ${exec_prefix}/libexec
transform=s,x,x,
EXECSUFFIX=

ATHENA = ../../..

INCTOP = $(ATHENA)/include

LIBTOP = $(ATHENA)/lib

LIBKAFS = -L$(top_builddir)/lib/kafs -lkafs 
LIBKRB	= -L$(LIBTOP)/krb -lkrb
LIBDES	= -L$(LIBTOP)/des -ldes
LIBOTP  = -L$(top_builddir)/lib/otp -lotp
LIBROKEN= -L$(LIBTOP)/roken -lroken

PROGS = ftpd$(EXECSUFFIX)

ftpd_SOURCES = ftpd.c ftpcmd.c logwtmp.c ls.c popen.c security.c krb4.c kauth.c
ftpd_OBJS = ftpd.o ftpcmd.o logwtmp.o ls.o popen.o security.o krb4.o kauth.o

SOURCES = $(ftpd_SOURCES)
OBJECTS = $(ftpd_OBJS)

all: $(PROGS)

$(ftpd_OBJS): security.h

security.c:
	$(LN_S) $(srcdir)/../ftp/security.c .
security.h:
	$(LN_S) $(srcdir)/../ftp/security.h .
krb4.c:
	$(LN_S) $(srcdir)/../ftp/krb4.c .
gssapi.c:
	$(LN_S) $(srcdir)/../ftp/gssapi.c .

.c.o:
	$(CC) -c -DFTP_SERVER -I. -I$(srcdir) -I$(srcdir)/../common -I$(INCTOP) $(DEFS) $(CFLAGS) $(CPPFLAGS) $<

install: all
	$(MKINSTALLDIRS) $(DESTDIR)$(libexecdir)
	for x in $(PROGS); do \
	  $(INSTALL_PROGRAM) $$x $(DESTDIR)$(libexecdir)/`echo $$x | sed '$(transform)'`; \
	done

uninstall:
	for x in $(PROGS); do \
	  rm -f $(DESTDIR)$(libexecdir)/`echo $$x | sed '$(transform)'`; \
	done

ftpd$(EXECSUFFIX): $(ftpd_OBJS)
	$(CC) $(LD_FLAGS) $(LDFLAGS) -o $@ $(ftpd_OBJS) -L../common -lcommon $(LIBKAFS) $(LIBKRB) $(LIBOTP) $(LIBDES) $(LIBROKEN) $(LIB_DBM) $(LIBS) $(LIBROKEN)

ftpcmd.c: ftpcmd.y
	$(YACC) $(YFLAGS) $<
	chmod a-w y.tab.c
	mv -f y.tab.c ftpcmd.c

TAGS: $(SOURCES)
	etags $(SOURCES)

CLEANFILES = ftpd$(EXECSUFFIX) ftpcmd.c security.c security.h krb4.c gssapi.c

clean cleandir:
	rm -f *~ *.o core \#* $(CLEANFILES)

distclean: 
	rm -f Makefile

.PHONY: all install uninstall clean cleandir distclean
