# $KTH: Makefile.am,v 1.5 2001/09/17 05:47:02 assar Exp $

include $(top_srcdir)/Makefile.am.common

INCLUDES += -DROKEN_RENAME

include_HEADERS =  krb_db.h kdc.h

build_HEADERZ = $(include_HEADERS)

lib_LTLIBRARIES = libkdb.la
libkdb_la_LDFLAGS = -version-info 1:10:0
libkdb_la_LIBADD  = ../krb/libkrb.la $(LIB_des)

libkdb_la_SOURCES =				\
	copykey.c				\
	krb_cache.c				\
	krb_dbm.c				\
	krb_kdb_utils.c				\
	krb_lib.c				\
	print_princ.c

$(libkdb_la_OBJECTS): ../../include/config.h roken_rename.h

roken_rename.h:
	$(LN_S) $(srcdir)/../krb/roken_rename.h .
