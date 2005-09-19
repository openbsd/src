#	$OpenBSD: Makefile,v 1.1.1.1 2005/09/19 03:34:18 kurt Exp $

LIB=		dd
SRCS=		dd.c
LDADD+=		-Wl,-E
LDADD+=		-Wl,-rpath,$(EE_OBJDIR)
LDADD+=		-L$(EE_OBJDIR) -lee

regress: all

.include <bsd.lib.mk>
