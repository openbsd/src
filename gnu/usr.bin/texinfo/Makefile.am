# Makefile.am for texinfo.
# $Id: Makefile.am,v 1.5 2006/07/17 16:12:35 espie Exp $
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
AUTOMAKE_OPTIONS = 1.8 readme-alpha

# Additional files to distribute.
EXTRA_DIST = ChangeLog.46 INSTALL.generic INTRODUCTION README.dev \
  config.rpath \
  djgpp/README djgpp/config.bat djgpp/config.sed djgpp/config.site

# This is to prevent texinfo.tex from being included in the top-level
# distribution directory.
TEXINFO_TEX = doc/texinfo.tex

if TOOLS_ONLY
  # Build native tools only.
  SUBDIRS = lib info makeinfo util
else
  # All subdirectories.
  # Do intl, m4, and lib first since the C programs depend on them.
  # Do doc last so makeinfo will be built when we get there.
  # Others are alphabetical.
  SUBDIRS = $(native_tools) intl m4 lib info makeinfo po util doc
endif

# for gettext.
ACLOCAL_AMFLAGS = -I m4

# In case TEXINFO_MAINT is set during make distcheck; in this case, the
# simple doc-related files generated in info/ make distcleancheck fail.
distcleancheck_listfiles = \
  find -type f -exec sh -c 'test -f $(srcdir)/{} || echo {}' ';'

# One special target for installers to use by hand if desired.
install-tex:
	cd doc && $(MAKE) TEXMF=$(TEXMF) install-tex

distclean-local:
	-test -d "$(native_tools)" && rm -rf "$(native_tools)"

# Don't install native tools for cross compile.
if TOOLS_ONLY
install:

endif
