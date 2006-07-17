# $Id: Makefile.am,v 1.5 2006/07/17 16:12:36 espie Exp $
# Makefile.am for texinfo/util.
# Run automake in .. to produce Makefile.in from this.
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

bin_PROGRAMS = ginstall-info texindex
bin_SCRIPTS = texi2dvi texi2pdf

# Use `ginstall-info' in the definition of PROGRAMS and in dependencies
# because automake generates an install-info target in every Makefile (a
# bug, IMHO, but this is easier than patching Automake.)
ginstall_info_SOURCES = install-info.c

# Transform `ginstall-info' to `install-info' before applying any
# user-specified name transformations.
transform = s/ginstall-info/install-info/; $(program_transform_name)

localedir = $(datadir)/locale
INCLUDES = -I$(top_srcdir)/lib -I../intl -DLOCALEDIR=\"$(localedir)\"
LDADD = ../lib/libtxi.a $(LIBINTL)

dist_pkgdata_DATA = texinfo.dtd texinfo.xsl
pkgdata_DATA = texinfo.cat

# we don't actually use version.texi in the rule, but it's one way of
# noticing when the version changes.
texinfo.cat: texinfo-cat.in $(top_srcdir)/doc/version.texi
	sed 's/__VERSION__/@VERSION@/g' $(srcdir)/texinfo-cat.in >$@

# Most of these are for fun.  The only official/installed ones are the
# texi2* scripts.
#
EXTRA_DIST = README deref.c dir-example fix-info-dir fixfonts \
  fixref.gawk gdoc gen-dir-node gendocs.sh gendocs_template infosrch \
  install-info-html outline.gawk \
  prepinfo.awk tex3patch texi-docstring-magic.el texi2dvi texi2pdf txitextest \
  $(dist_pkgdata_DATA) texinfo-cat.in
CLEANFILES = $(pkgdata_DATA)
