# $KTH: Makefile.am,v 1.5 2001/08/28 08:42:58 assar Exp $

include $(top_srcdir)/Makefile.am.common

INCLUDES += $(INCLUDE_hesiod) $(INCLUDE_des)

libexec_PROGRAMS = movemail

movemail_SOURCES = movemail.c pop.c

man_MANS = movemail.8

LDADD = \
	$(top_builddir)/lib/krb/libkrb.la	\
	$(LIB_des)	\
	$(LIB_roken)				\
	$(LIB_hesiod)
