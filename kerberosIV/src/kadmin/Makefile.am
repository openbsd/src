# $KTH: Makefile.am,v 1.4 2001/08/28 08:43:00 assar Exp $

include $(top_srcdir)/Makefile.am.common

INCLUDES += $(INCLUDE_des)

bin_PROGRAMS = kadmin kpasswd
sbin_PROGRAMS = ksrvutil
libexec_PROGRAMS = kadmind

LDADD = \
	$(top_builddir)/lib/kadm/libkadm.la	\
	$(top_builddir)/lib/krb/libkrb.la	\
	$(LIB_des)	\
	$(top_builddir)/lib/com_err/libcom_err.la	\
	$(LIB_roken)

SOURCES = kpasswd.c kadmin.c kadm_server.c kadm_funcs.c pw_check.c \
          admin_server.c kadm_ser_wrap.c ksrvutil.c ksrvutil_get.c \
	  new_pwd.c random_password.c

kpasswd_SOURCES = kpasswd.c new_pwd.c

kadmin_SOURCES = kadmin.c new_pwd.c random_password.c

kadmin_LDADD = $(LDADD)\
	$(top_builddir)/lib/sl/libsl.la \
	$(LIB_readline)

kadmind_SOURCES = kadm_server.c kadm_funcs.c admin_server.c \
	kadm_ser_wrap.c pw_check.c

kadmind_LDADD = \
	$(top_builddir)/lib/kdb/libkdb.la \
	$(top_builddir)/lib/acl/libacl.la \
	$(LDADD) \
	$(CRACKLIB) \
	$(LIB_roken) \
	$(DBLIB)

ksrvutil_SOURCES = ksrvutil.c ksrvutil_get.c
