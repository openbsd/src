# Generated automatically from Makefile.in by configure.
#
# $KTH: Makefile.in,v 1.30.16.1 2000/06/23 03:20:03 assar Exp $
#

SHELL = /bin/sh

srcdir = .

CC = gcc
LINK = $(CC)
AR = ar
RANLIB = ranlib
LN_S = ln -s
DEFS = -DHAVE_CONFIG_H -DROKEN_RENAME
CFLAGS =  -g -O2 $(WFLAGS)
WFLAGS = 
LDFLAGS = -g
LD_FLAGS = 
LIBS = -lutil  
LIB_DBM = 

INSTALL = /usr/bin/install -c
INSTALL_DATA	= ${INSTALL} -m 644
MKINSTALLDIRS = ../../mkinstalldirs

prefix = /usr/athena
exec_prefix = ${prefix}
libdir = ${exec_prefix}/lib

PICFLAGS = 
 
LIB_DEPS =  $(LIB_DBM) -lc
build_symlink_command   = @true
install_symlink_command = @true

LIBNAME = $(LIBPREFIX)otp
LIBEXT = a
SHLIBEXT = nobuild
LIBPREFIX = lib
LDSHARED = ld -Bshareable
EXECSUFFIX=
LIB = $(LIBNAME).$(LIBEXT)
PROGS = otptest$(EXECSUFFIX)

LIB_SOURCES = otp.c otp_challenge.c otp_db.c otp_md.c \
	  otp_parse.c otp_print.c otp_verify.c

EXTRA_LIB_SOURCES = snprintf.c strlwr.c strcasecmp.c

OTPTEST_SOURCES = otptest.c

SOURCES = $(LIB_SOURCES) $(EXTRA_LIB_SOURCES) $(OTPTEST_SOURCES)

LIB_OBJECTS = otp.o otp_challenge.o otp_db.o otp_md.o \
	  otp_parse.o otp_print.o otp_verify.o

LIBADD = \
	snprintf.o \
	strcasecmp.o \
	strlwr.o

OTPTEST_OBJECTS = otptest.o

OBJECTS = $(LIB_OBJECTS) $(LIBADD) $(OTPTEST_OBJECTS)
LIBROKEN=-L../../lib/roken -lroken

all: $(LIB) $(PROGS)

Wall:
	make CFLAGS="-g -Wall -Wno-comment -Wmissing-prototypes -Wmissing-declarations -D__USE_FIXED_PROTOTYPES__"

.c.o:
	$(CC) -c $(DEFS) -I../../include -I$(srcdir) -I$(srcdir)/../des $(CFLAGS) $(CPPFLAGS) $(PICFLAGS) $<

install: all
	$(MKINSTALLDIRS) $(DESTDIR)$(libdir)
	$(INSTALL_DATA)  $(LIB) $(DESTDIR)$(libdir)/$(LIB)
	@true

uninstall:
	rm -f $(DESTDIR)$(libdir)/$(LIB)

TAGS: $(SOURCES)
	etags $(SOURCES)

check:	otptest$(EXECSUFFIX)
	./otptest$(EXECSUFFIX)

clean:
	rm -f $(LIB) $(PROGS) *.o *.a *.so *.so.* so_locations

mostlyclean: clean

distclean: clean
	rm -f Makefile *~

realclean: distclean
	rm -f TAGS $(EXTRA_LIB_SOURCES)

otptest$(EXECSUFFIX): $(OTPTEST_OBJECTS) $(LIB)
	$(LINK) $(LD_FLAGS) $(LDFLAGS) -o $@ $(OTPTEST_OBJECTS) -L. -lotp -L../des -ldes $(LIBROKEN) $(LIB_DBM) $(LIBS) $(LIBROKEN)

$(LIBNAME).a: $(LIB_OBJECTS) $(LIBADD)
	rm -f $@
	$(AR) cr $@ $(LIB_OBJECTS) $(LIBADD)
	-$(RANLIB) $@

$(LIBNAME).$(SHLIBEXT): $(LIB_OBJECTS) $(LIBADD)
	rm -f $@
	$(LDSHARED) -o $@ $(LIB_OBJECTS) $(LIBADD) $(LIB_DEPS)
	@true

$(OBJECTS): ../../include/config.h

snprintf.c:
	$(LN_S) $(srcdir)/../roken/snprintf.c .
strcasecmp.c:
	$(LN_S) $(srcdir)/../roken/strcasecmp.c .
strlwr.c:
	$(LN_S) $(srcdir)/../roken/strlwr.c .

.PHONY: all Wall install uninstall check clean mostlyclean distclean realclean
