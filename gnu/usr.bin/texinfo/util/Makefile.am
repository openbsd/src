## Makefile.am for texinfo/util.
## $Id: Makefile.am,v 1.1.1.1 1997/08/01 22:01:03 kstailey Exp $
## Run automake in .. to produce Makefile.in from this.

bin_PROGRAMS = install-info texindex
bin_SCRIPTS = texi2dvi

localedir = $(datadir)/locale
INCLUDES = -I$(top_srcdir)/lib -I../intl -DLOCALEDIR=\"$(localedir)\"
LDADD = ../lib/libtxi.a @INTLLIBS@

EXTRA_DIST = README deref.c fixfonts gen-dir-node tex3patch texi2dvi \
             update-info 
