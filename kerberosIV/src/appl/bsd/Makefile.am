# $KTH: Makefile.am,v 1.8 2001/09/17 03:02:07 assar Exp $

include $(top_srcdir)/Makefile.am.common

INCLUDES += -DBINDIR='"$(bindir)"' $(INCLUDE_des)

bin_PROGRAMS = rsh rcp rlogin su login
bin_SUIDS = rsh rcp rlogin su
libexec_PROGRAMS = rshd rlogind

man_MANS = login.1 login.access.5 rcp.1 rlogin.1 rlogind.8 rsh.1 \
	rshd.8 su.1

LDADD = \
	libbsd.a				\
	$(LIB_krb4) \
	$(LIB_des)	\
	$(LIB_roken)

noinst_LIBRARIES = libbsd.a

libbsd_a_SOURCES = kcmd.c krcmd.c encrypt.c rcmd_util.c osfc2.c

rcp_SOURCES	= rcp.c rcp_util.c 
login_SOURCES 	= login.c klogin.c login_fbtab.c login_access.c \
		  sysv_default.c sysv_environ.c sysv_shadow.c \
		  utmp_login.c utmpx_login.c stty_default.c tty.c
rlogind_SOURCES	= rlogind.c forkpty.c tty.c


rcp_LDADD = \
	$(LIB_kafs) \
	$(LDADD) \
	$(LIB_security)

rshd_LDADD = $(rcp_LDADD)

rlogind_LDADD = \
	$(LIB_kafs) \
	$(LIB_logwtmp) \
	$(LDADD)

login_LDADD = \
	$(LIB_setpcred) \
	$(LIB_otp) \
	$(rcp_LDADD) \
	$(LIB_odm_initialize) \
	$(LIB_getattr)

su_LDADD = $(rcp_LDADD)
