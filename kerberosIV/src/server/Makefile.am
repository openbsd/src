# $KTH: Makefile.am,v 1.5 2001/08/28 08:43:03 assar Exp $

include $(top_srcdir)/Makefile.am.common

INCLUDES += $(INCLUDE_des)

libexec_PROGRAMS = kerberos

man_MANS = kerberos.8

LDADD = \
	$(top_builddir)/lib/kdb/libkdb.la	\
	$(top_builddir)/lib/krb/libkrb.la	\
	$(LIB_des)	\
	$(LIB_roken)				\
	$(DBLIB)
