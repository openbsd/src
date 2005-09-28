# $OpenBSD: Makefile,v 1.1.1.1 2005/09/28 15:42:32 kurt Exp $

PROG=		prog2
SRCS=		main.c
LDFLAGS+=	-Wl,-E
LDFLAGS+=	-Wl,-rpath,$(AA_OBJDIR)
LDFLAGS+=	-Wl,-rpath,$(BB_OBJDIR)

.include <bsd.regress.mk>
