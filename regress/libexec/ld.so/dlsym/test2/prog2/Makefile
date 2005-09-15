# $OpenBSD: Makefile,v 1.1.1.1 2005/09/15 22:16:27 kurt Exp $

.include <../Makefile.inc>

PROG=		prog2
SRCS=		main.c
LDADD=		-Wl,-E -Wl,-rpath,$(AA_OBJDIR) -L$(AA_OBJDIR) -laa

.include <bsd.regress.mk>
