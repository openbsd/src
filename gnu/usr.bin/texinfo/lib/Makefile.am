# $Id: Makefile.am,v 1.5 2002/12/13 03:12:26 millert Exp $
# Makefile.am for texinfo/lib.
# Run automake in .. to produce Makefile.in from this.
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

noinst_LIBRARIES = libtxi.a
INCLUDES = -I../intl

# Don't need to list alloca.c, etc., Automake includes them.
libtxi_a_SOURCES = gettext.h substring.c system.h xexit.c xmalloc.c xstrdup.c
libtxi_a_LIBADD = @LIBOBJS@ @ALLOCA@
libtxi_a_DEPENDENCIES = $(libtxi_a_LIBADD)

## xx configure for bzero?, clib, other common stuff

EXTRA_DIST = README
