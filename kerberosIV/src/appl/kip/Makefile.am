# $KTH: Makefile.am,v 1.3 2000/11/15 23:58:18 assar Exp $

include $(top_srcdir)/Makefile.am.common

INCLUDES += -DLIBEXECDIR="\"$(libexecdir)\""

bin_PROGRAMS = kip
libexec_PROGRAMS = kipd
libexec_SCRIPTS = kip-join-network kipd-control
CLEANFILES = $(libexec_SCRIPTS)

kip_SOURCES = kip.c common.c
kipd_SOURCES = kipd.c common.c

LDADD = \
	$(top_builddir)/lib/krb/libkrb.la	\
	$(LIB_des)	\
	$(LIB_roken)

kip-join-network: kip-join-network.in
	sed -e "s!%bindir%!$(bindir)!" $(srcdir)/kip-join-network.in > $@
	chmod +x $@

kipd-control: kipd-control.in
	sed -e "s!%bindir%!$(bindir)!" $(srcdir)/kipd-control.in > $@
	chmod +x $@

