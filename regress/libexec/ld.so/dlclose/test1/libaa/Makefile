#	$OpenBSD: Makefile,v 1.1.1.1 2005/09/28 15:42:32 kurt Exp $

LIB=		aa
SRCS=		aa.c
LDADD+=		-Wl,-E
LDADD+=		-Wl,-rpath,$(BB_OBJDIR)
LDADD+=		-L$(BB_OBJDIR) -lbb

regress: all

.include <bsd.lib.mk>
