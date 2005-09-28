# $OpenBSD: Makefile,v 1.1.1.1 2005/09/28 15:42:32 kurt Exp $

PROG=		prog1
SRCS=		main.c
LDFLAGS+=	-Wl,-E
LDFLAGS+=	-Wl,-rpath,$(AA_OBJDIR)

.include <bsd.regress.mk>
