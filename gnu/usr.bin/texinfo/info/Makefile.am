# $Id: Makefile.am,v 1.4 2002/06/10 13:51:03 espie Exp $
# Makefile.am for texinfo/info.
# Run automake in .. to produce Makefile.in from this.
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

noinst_PROGRAMS = makedoc

# Use `ginfo' for building to avoid confusion with the standard `info'
# target.  Removes the `g' in the install rule before applying any
# user-specified name transformations.
bin_PROGRAMS = ginfo infokey
transform = s/ginfo/info/; @program_transform_name@

localedir = $(datadir)/locale

# -I. for funs.h.
# Automake puts -I.. and -I$(srcdir) into DEFS by default, but
# we need to override it, so include them ourselves.
INCLUDES = -I. -I$(top_srcdir)/lib -I../intl -I.. -I$(srcdir)
DEFS = -DINFODIR=\"$(infodir)\" -DLOCALEDIR=\"$(localedir)\" @DEFS@
LDADD = ../lib/libtxi.a @TERMLIBS@ @LIBINTL@

EXTRA_DIST = README pcterm.c

if TEXINFO_MAINT
# The files `doc.c' and `funs.h' are created by ./makedoc run over the source
# files which contain DECLARE_INFO_COMMAND.  `funs.h' is a header file
# listing the functions found.  `doc.c' is a structure containing pointers
# to those functions along with completable names and documentation strings.
BUILT_SOURCES = doc.c funs.h key.c
else
# don't make installers rebuild them.
generated_sources = doc.c funs.h key.c
endif

makedoc_SOURCES = makedoc.c
ginfo_SOURCES = dir.c display.c display.h doc.h dribble.c dribble.h \
  echo-area.c echo-area.h \
  filesys.c filesys.h footnotes.c footnotes.h gc.c gc.h \
  indices.c indices.h info-utils.c info-utils.h info.c info.h infodoc.c \
  infomap.c infomap.h m-x.c man.c man.h nodemenu.c nodes.c nodes.h \
  search.c search.h session.c session.h signals.c signals.h \
  termdep.h terminal.c terminal.h tilde.c tilde.h \
  variables.c variables.h window.c window.h \
  $(BUILT_SOURCES) $(generated_sources)
infokey_SOURCES = infokey.c infokey.h key.c key.h

# Files with Info commands defined that makedoc should read.
cmd_sources = $(srcdir)/session.c $(srcdir)/echo-area.c $(srcdir)/infodoc.c \
  $(srcdir)/m-x.c $(srcdir)/indices.c $(srcdir)/nodemenu.c \
  $(srcdir)/footnotes.c $(srcdir)/variables.c

# The $(EXEEXT) should be added by Automake, but isn't. Fine.
$(BUILT_SOURCES): makedoc$(EXEEXT) $(cmd_sources)
# This is insufficient.  We really need them not to be in the
# distribution in the first place, but it seems Automake does not
# currently allow that.
	rm -f $(BUILT_SOURCES)
	./makedoc $(cmd_sources)
