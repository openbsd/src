# $OpenBSD: Makefile,v 1.2 2005/09/18 11:56:35 kurt Exp $

.include <../Makefile.inc>

PROG=		prog3
SRCS=		main.c
LDADD=		-Wl,-E -Wl,-rpath,$(AA_OBJDIR) -Wl,-rpath,$(BB_OBJDIR) -L$(AA_OBJDIR) -laa

.include <bsd.regress.mk>
