# $KTH: Makefile.am,v 1.11 1999/11/17 16:07:04 assar Exp $

include $(top_srcdir)/Makefile.am.common

INCLUDES += $(INCLUDE_krb4) $(X_CFLAGS)

WFLAGS += $(WFLAGS_NOIMPLICITINT)

X_PROGS_BIN_PROGS = xnlock

MAKE_X_PROGS_BIN_PROGS     = @MAKE_X_PROGS_BIN_PROGS@

bin_PROGRAMS   = $(MAKE_X_PROGS_BIN_PROGS)
man1_MANS = xnlock.man

EXTRA_PROGRAMS         = $(X_PROGS_BIN_PROGS)

EXTRA_DIST = $(man1_MANS) nose.0.left nose.0.right nose.1.left nose.1.right \
	nose.down nose.front nose.left.front nose.right.front

LDADD = \
	$(LIB_kafs) \
	$(LIB_krb5) \
	$(LIB_krb4) \
	$(top_builddir)/lib/des/libdes.la \
	$(LIB_roken) \
	$(X_LIBS) -lXt $(X_PRE_LIBS) -lX11 $(X_EXTRA_LIBS)
