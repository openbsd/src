# $OpenBSD: Makefile,v 1.1.1.1 2005/09/30 15:14:46 kurt Exp $

PROG=		prog3
SRCS=		main.c
LDFLAGS+=	-Wl,-E
LDFLAGS+=	-Wl,-rpath,$(AA_OBJDIR)
LDFLAGS+=	-Wl,-rpath,$(BB_OBJDIR)

.include <bsd.regress.mk>
