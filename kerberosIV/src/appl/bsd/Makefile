# Generated automatically from Makefile.in by configure.
# $KTH: Makefile.in,v 1.68 1999/03/27 17:05:34 joda Exp $

SHELL = /bin/sh

srcdir = .
top_srcdir = ../..

top_builddir = ../..

CC = gcc
LINK = $(CC)
AR = ar
RANLIB = ranlib
DEFS = -DHAVE_CONFIG_H -DBINDIR='"$(bindir)"'
CFLAGS =  -g -O2 $(WFLAGS)
WFLAGS = 
LD_FLAGS = 
INSTALL = /usr/bin/install -c
INSTALL_PROGRAM = ${INSTALL}
LIBS = -lutil  
LIB_DBM = 
MKINSTALLDIRS = ../../mkinstalldirs

prefix = /usr/athena
exec_prefix = ${prefix}
libdir = ${exec_prefix}/lib
libexecdir = ${exec_prefix}/libexec
bindir = ${exec_prefix}/bin
transform=s,x,x,
EXECSUFFIX=

# Beware, these are all setuid root programs
PROG_SUIDBIN	= rsh$(EXECSUFFIX) \
		  rcp$(EXECSUFFIX) \
		  rlogin$(EXECSUFFIX) \
		  su$(EXECSUFFIX)
PROG_BIN	= login$(EXECSUFFIX)
PROG_LIBEXEC	= rshd$(EXECSUFFIX) \
		  rlogind$(EXECSUFFIX)
PROGS = $(PROG_SUIDBIN) $(PROG_BIN) $(PROG_LIBEXEC)

SOURCES = rsh.c kcmd.c krcmd.c rlogin.c rcp.c rcp_util.c rshd.c \
	login.c klogin.c login_access.c su.c rlogind.c \
	login_fbtab.c forkpty.c sysv_default.c sysv_environ.c sysv_shadow.c \
	utmp_login.c utmpx_login.c stty_default.c encrypt.c rcmd_util.c tty.c \
	osfc2.c

rsh_OBJS	= rsh.o kcmd.o krcmd.o encrypt.o rcmd_util.o
rcp_OBJS	= rcp.o rcp_util.o kcmd.o krcmd.o encrypt.o rcmd_util.o osfc2.o
rlogin_OBJS	= rlogin.o kcmd.o krcmd.o encrypt.o rcmd_util.o
login_OBJS 	= login.o klogin.o login_fbtab.o login_access.o \
		  sysv_default.o sysv_environ.o sysv_shadow.o \
		  utmp_login.o utmpx_login.o stty_default.o tty.o osfc2.o
su_OBJS		= su.o
rshd_OBJS	= rshd.o encrypt.o rcmd_util.o osfc2.o
rlogind_OBJS	= rlogind.o forkpty.o encrypt.o rcmd_util.o tty.o


all: $(PROGS) 

Wall:
	make CFLAGS="-g -Wall -Wno-comment -Wmissing-prototypes -Wmissing-declarations -D__USE_FIXED_PROTOTYPES__"

.c.o: 
	$(CC) -c $(DEFS) -I../../include -I$(srcdir) $(CFLAGS) $(CPPFLAGS) $<

install: all
	$(MKINSTALLDIRS) $(DESTDIR)$(libexecdir)
	for x in $(PROG_LIBEXEC); do \
	  $(INSTALL_PROGRAM) $$x $(DESTDIR)$(libexecdir)/`echo $$x| sed '$(transform)'`; \
	done
	$(MKINSTALLDIRS) $(DESTDIR)$(bindir)
	for x in $(PROG_BIN); do \
	  $(INSTALL_PROGRAM) $$x $(DESTDIR)$(bindir)/`echo $$x| sed '$(transform)'`; \
	done
	-for x in $(PROG_SUIDBIN); do \
	  $(INSTALL_PROGRAM) -o root -m 04555 $$x $(DESTDIR)$(bindir)/`echo $$x| sed '$(transform)'`; \
	done

uninstall:
	for x in $(PROG_LIBEXEC); do \
	  rm -f $(DESTDIR)$(libexecdir)/`echo $$x| sed '$(transform)'`; \
	done
	for x in $(PROG_BIN); do \
	  rm -f $(DESTDIR)$(bindir)/`echo $$x| sed '$(transform)'`; \
	done
	for x in $(PROG_SUIDBIN); do \
	  rm -f $(DESTDIR)$(bindir)/`echo $$x| sed '$(transform)'`; \
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

KLIB=-L../../lib/krb -lkrb -L../../lib/des -ldes
KLIB_AFS=-L$(top_builddir)/lib/kafs -lkafs  $(KLIB)
OTPLIB=-L$(top_builddir)/lib/otp -lotp
LIBROKEN=-L../../lib/roken -lroken

LIB_security=

rcp$(EXECSUFFIX): $(rcp_OBJS)
	$(LINK) $(LD_FLAGS) $(LDFLAGS) -o $@ $(rcp_OBJS) $(KLIB_AFS) $(LIBROKEN) $(LIBS) $(LIBROKEN) $(LIB_security)

rsh$(EXECSUFFIX): $(rsh_OBJS)
	$(LINK) $(LD_FLAGS) $(LDFLAGS) -o $@ $(rsh_OBJS) $(KLIB) $(LIBROKEN) $(LIBS) $(LIBROKEN)

rshd$(EXECSUFFIX): $(rshd_OBJS)
	$(LINK) $(LD_FLAGS) $(LDFLAGS) -o $@ $(rshd_OBJS) $(KLIB_AFS) $(LIBROKEN) $(LIBS) $(LIBROKEN) $(LIB_security)

rlogin$(EXECSUFFIX): $(rlogin_OBJS)
	$(LINK) $(LD_FLAGS) $(LDFLAGS) -o $@  $(rlogin_OBJS) $(KLIB) $(LIBROKEN) $(LIBS) $(LIBROKEN)

rlogind$(EXECSUFFIX): $(rlogind_OBJS)
	$(LINK) $(LD_FLAGS) $(LDFLAGS) -o $@ $(rlogind_OBJS) $(KLIB_AFS) $(LIBROKEN) $(LIBS) $(LIBROKEN)

login$(EXECSUFFIX): $(login_OBJS)
	$(LINK) $(LD_FLAGS) $(LDFLAGS) -o $@ $(login_OBJS) $(OTPLIB) $(KLIB_AFS) $(LIBROKEN) $(LIB_DBM) $(LIBS) $(LIBROKEN) $(LIB_security)

su$(EXECSUFFIX): $(su_OBJS)
	$(LINK) $(LD_FLAGS) $(LDFLAGS) -o $@ $(su_OBJS) $(KLIB_AFS) $(LIBROKEN) $(LIBS) $(LIBROKEN)

.PHONY: all Wall install uninstall check clean mostlyclean distclean realclean
