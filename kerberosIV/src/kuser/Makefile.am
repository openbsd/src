# $KTH: Makefile.am,v 1.4 2000/11/15 23:58:20 assar Exp $

include $(top_srcdir)/Makefile.am.common

bin_PROGRAMS = kinit kdestroy klist

man_MANS = kinit.1 kdestroy.1 klist.1

LDADD = \
	$(LIB_krb4)	\
	$(LIB_des)	\
	$(LIB_roken)

klist_LDADD = \
	$(LIB_kafs)	\
	$(LDADD)

kdestroy_LDADD = \
	$(LIB_kafs)	\
	$(LDADD)
