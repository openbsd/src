# $KTH: Makefile.am,v 1.10 1999/08/04 17:30:12 assar Exp $

include $(top_srcdir)/Makefile.am.common

man_MANS = editline.3

lib_LIBRARIES = libeditline.a
if el_compat
noinst_LIBRARIES = libel_compat.a
else
noinst_LIBRARIES =
endif

noinst_PROGRAMS = testit

CHECK_LOCAL =

testit_LDADD = \
	libeditline.a \
	$(LIB_tgetent) \
	$(LIB_roken)

include_HEADERS = editline.h

libeditline_a_SOURCES = complete.c editline.c sysunix.c \
	editline.h roken_rename.h unix.h
libel_compat_a_SOURCES = edit_compat.c

EXTRA_DIST = $(man_MANS)
