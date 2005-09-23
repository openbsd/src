#	$OpenBSD: Makefile,v 1.1.1.1 2005/09/23 19:02:29 kurt Exp $

PROG=		prog1
SRCS=		main.c
LDFLAGS+=	-Wl,-E
LDFLAGS+=	-Wl,-rpath,$(AA_OBJDIR)
LDFLAGS+=	-Wl,-rpath,$(BB_OBJDIR)

.include <bsd.regress.mk>
