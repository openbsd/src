# Makefile.am for texinfo.
# $Id: Makefile.am,v 1.4 2002/06/10 13:51:01 espie Exp $
# Process this file with automake to produce Makefile.in in all directories.
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

# Be sure we're using the right version of Automake.
AUTOMAKE_OPTIONS = 1.4 readme-alpha

# Additional files to distribute.
EXTRA_DIST = COPYING.DOC INTRODUCTION config.rpath dir-example djgpp

# This is to prevent texinfo.tex from being included in the top-level
# distribution directory.
TEXINFO_TEX = doc/texinfo.tex

# All subdirectories.
# Do intl, m4, and lib first since the C programs depend on them.
# Do doc last so makeinfo will be built when we get there.
# Others are alphabetical.
SUBDIRS = intl m4 lib info makeinfo po util doc

# for gettext.
ACLOCAL_AMFLAGS = -I m4

# One special target for them to use by hand.
install-tex:
	cd doc && $(MAKE) TEXMF=$(TEXMF) install-tex
