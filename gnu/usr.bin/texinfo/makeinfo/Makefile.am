## Makefile.am for texinfo/makeinfo.
## $Id: Makefile.am,v 1.3 2000/02/09 02:18:41 espie Exp $
## Run automake in .. to produce Makefile.in from this.

bin_PROGRAMS = makeinfo

localedir = $(datadir)/locale
INCLUDES = -I$(top_srcdir)/lib -I../intl -DLOCALEDIR=\"$(localedir)\"
LDADD = ../lib/libtxi.a @INTLLIBS@

makeinfo_SOURCES = \
  cmds.c cmds.h defun.c defun.h files.c files.h footnote.c footnote.h \
  html.c html.h index.c index.h insertion.c insertion.h lang.c lang.h \
  macro.c macro.h makeinfo.c makeinfo.h multi.c node.c node.h \
  sectioning.c sectioning.h toc.c toc.h

EXTRA_DIST = README

SUBDIRS = tests
