# $KTH: Makefile.am,v 1.4 2001/09/17 05:46:59 assar Exp $

include $(top_srcdir)/Makefile.am.common

INCLUDES += -DROKEN_RENAME

include_HEADERS =  acl.h

build_HEADERZ = $(include_HEADERS)

lib_LTLIBRARIES = libacl.la
libacl_la_LDFLAGS = -version-info 1:0:0
libacl_la_LIBADD  = ../krb/libkrb.la

libacl_la_SOURCES = acl_files.c

man_MANS = acl_check.3

$(OBJECTS): roken_rename.h

roken_rename.h:
	$(LN_S) $(srcdir)/../krb/roken_rename.h .

