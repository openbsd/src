# $KTH: Makefile.am,v 2.3 2000/11/15 23:58:21 assar Exp $

include $(top_srcdir)/Makefile.am.common

libexec_PROGRAMS = kprop kpropd

man_MANS = kprop.8 kpropd.8

LDADD = \
	$(top_builddir)/lib/krb/libkrb.la	\
	$(LIB_des)	\
	$(LIB_roken)

INCLUDES += -DSBINDIR=\"$(sbindir)\"
