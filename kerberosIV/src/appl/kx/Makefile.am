# $KTH: Makefile.am,v 1.9 1999/10/10 04:45:18 assar Exp $

include $(top_srcdir)/Makefile.am.common

INCLUDES += $(INCLUDE_krb4) $(X_CFLAGS)

WFLAGS += $(WFLAGS_NOIMPLICITINT)

X_PROGS_BIN_PROGS     = kx
X_PROGS_BIN_SCRPTS    = rxterm rxtelnet tenletxr
X_PROGS_LIBEXEC_PROGS = kxd

CLEANFILES = rxterm rxtelnet tenletxr

MAKE_X_PROGS_BIN_PROGS     = @MAKE_X_PROGS_BIN_PROGS@
MAKE_X_PROGS_BIN_SCRPTS    = @MAKE_X_PROGS_BIN_SCRPTS@
MAKE_X_PROGS_LIBEXEC_PROGS = @MAKE_X_PROGS_LIBEXEC_PROGS@

bin_PROGRAMS     = $(MAKE_X_PROGS_BIN_PROGS)
bin_SCRIPTS      = $(MAKE_X_PROGS_BIN_SCRPTS)
libexec_PROGRAMS = $(MAKE_X_PROGS_LIBEXEC_PROGS)

EXTRA_PROGRAMS         = $(X_PROGS_BIN_PROGS) $(X_PROGS_LIBEXEC_PROGS)
EXTRA_SCRIPTS          = $(X_PROGS_BIN_SCRPTS)

if NEED_WRITEAUTH
XauWriteAuth_c = writeauth.c
endif

kx_SOURCES = \
	kx.c \
	kx.h \
	common.c \
	context.c \
	krb4.c \
	krb5.c \
	$(XauWriteAuth_c)

EXTRA_kx_SOURCES = writeauth.c

kxd_SOURCES = \
	kxd.c \
	kx.h \
	common.c \
	context.c \
	krb4.c \
	krb5.c \
	$(XauWriteAuth_c)

EXTRA_kxd_SOURCES = writeauth.c

EXTRA_DIST = rxterm.in rxtelnet.in tenletxr.in

rxterm: rxterm.in
	sed -e "s!%bindir%!$(bindir)!" $(srcdir)/rxterm.in > $@
	chmod +x $@

rxtelnet: rxtelnet.in
	sed -e "s!%bindir%!$(bindir)!" $(srcdir)/rxtelnet.in > $@
	chmod +x $@

tenletxr: tenletxr.in
	sed -e "s!%bindir%!$(bindir)!" $(srcdir)/tenletxr.in > $@
	chmod +x $@

LDADD = \
	$(LIB_kafs)				\
	$(LIB_krb5) \
	$(LIB_krb4)				\
	$(top_builddir)/lib/des/libdes.la	\
	$(LIB_roken)				\
	$(X_LIBS) $(LIB_XauReadAuth) $(X_PRE_LIBS) $(X_EXTRA_LIBS)
