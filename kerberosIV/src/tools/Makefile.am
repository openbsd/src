# $KTH: Makefile.am,v 1.2 2001/09/17 02:10:51 assar Exp $

include $(top_srcdir)/Makefile.am.common

EXTRA_DIST = krb4-config.1

CLEANFILES = krb4-config

bin_SCRIPTS = krb4-config

man_MANS = krb4-config.1

krb4-config: krb4-config.in
	sed	-e "s;@PACKAGE\@;$(PACKAGE);g" \
		-e "s;@VERSION\@;$(VERSION);g" \
		-e "s;@prefix\@;$(prefix);g" \
		-e "s;@exec_prefix\@;$(exec_prefix);g" \
		-e "s;@libdir\@;$(libdir);g" \
		-e "s;@includedir\@;$(includedir);g" \
		-e "s;@LIB_des_appl\@;$(LIB_des_appl);g" \
		-e "s;@LIBS\@;$(LIBS);g" \
		$(srcdir)/krb4-config.in > $@
	chmod +x $@
