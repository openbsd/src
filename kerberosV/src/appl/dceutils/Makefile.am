# $KTH: Makefile.am,v 1.5 2000/12/10 08:19:39 assar Exp $

include $(top_srcdir)/Makefile.am.common


DFSPROGS = k5dcecon
if AIX
AIX_DFSPROGS = dpagaix
endif

libexec_PROGRAMS = $(DFSPROGS) $(AIX_DFSPROGS)

dpagaix_CFLAGS = @dpagaix_CFLAGS@
dpagaix_LDFLAGS = @dpagaix_LDFLAGS@
dpagaix_LDADD = @dpagaix_LDADD@

LIB_dce = -ldce

k5dcecon_SOURCES  = k5dcecon.c k5dce.h

dpagaix_SOURCES = dpagaix.c

if IRIX
LDADD = $(LIB_dce)
else
LDADD = $(LIB_roken) $(LIB_dce)
endif
