## Makefile.am for texinfo/makeinfo.
## $Id: Makefile.am,v 1.2 1999/01/11 16:38:11 espie Exp $
## Run automake in .. to produce Makefile.in from this.

bin_PROGRAMS = makeinfo

localedir = $(datadir)/locale
INCLUDES = -I$(top_srcdir)/lib -I../intl -DLOCALEDIR=\"$(localedir)\"
LDADD = ../lib/libtxi.a @INTLLIBS@

makeinfo_SOURCES = makeinfo.c makeinfo.h multi.c

EXTRA_DIST = README
