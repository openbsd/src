## Makefile.am for texinfo.
## $Id: Makefile.am,v 1.3 2000/02/09 02:18:35 espie Exp $
## Process this file with automake to produce Makefile.in in all directories.

# Be sure we're using the right version of Automake.
AUTOMAKE_OPTIONS = 1.4 readme-alpha

# Additional files to distribute.
EXTRA_DIST = INTRODUCTION dir-example djgpp

# This is to prevent texinfo.tex from being included in the top-level
# distribution directory.
TEXINFO_TEX = doc/texinfo.tex

# All subdirectories.
# Do intl/ and lib/ first since the C programs depend on them.
# Do doc/ last so makeinfo will be built when we get there.
# Others are alphabetical.
SUBDIRS = intl lib info makeinfo po util doc

# One special target for them to use by hand.
install-tex:
	cd doc && $(MAKE) TEXMF=$(TEXMF) install-tex
