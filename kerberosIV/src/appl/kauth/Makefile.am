# $KTH: Makefile.am,v 1.12 2001/08/28 22:08:22 assar Exp $

include $(top_srcdir)/Makefile.am.common

INCLUDES += $(INCLUDE_krb4) -DBINDIR=\"$(bindir)\"

bin_PROGRAMS = kauth
bin_SCRIPTS = ksrvtgt
libexec_PROGRAMS = kauthd
CLEANFILES = $(bin_SCRIPTS)

man_MANS = kauth.1 ksrvtgt.1 kauthd.8 

EXTRA_DIST = zrefresh ksrvtgt.in

kauth_SOURCES = \
	kauth.c \
	kauth.h \
	rkinit.c \
	marshall.c \
	encdata.c

kauthd_SOURCES = \
	kauthd.c \
	kauth.h \
	marshall.c \
	encdata.c

ksrvtgt: ksrvtgt.in
	sed -e "s!%bindir%!$(bindir)!" $(srcdir)/ksrvtgt.in > $@
	chmod +x $@

install-exec-local:
	if test -f $(DESTDIR)$(bindir)/zrefresh -o -r $(DESTDIR)$(bindir)/zrefresh; then \
	  true; \
	else \
	  $(INSTALL_SCRIPT) $(srcdir)/zrefresh $(DESTDIR)$(bindir)/`echo zrefresh | sed '$(transform)'`; \
	fi

LDADD = \
	$(LIB_kafs) \
	$(LIB_krb4) \
	$(LIB_des) \
	$(LIB_roken)
