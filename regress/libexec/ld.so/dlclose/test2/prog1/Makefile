# $OpenBSD: Makefile,v 1.1.1.1 2005/09/30 14:51:52 kurt Exp $

PROG=		prog1
SRCS=		main.c
LDFLAGS+=	-Wl,-E
LDFLAGS+=	-Wl,-rpath,$(AA_OBJDIR)

.include <bsd.regress.mk>
