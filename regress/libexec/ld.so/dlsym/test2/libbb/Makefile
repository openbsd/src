# $OpenBSD: Makefile,v 1.2 2005/09/18 12:17:29 kurt Exp $

.include <../Makefile.inc>

LIB=bb
SRCS= bb.c
LDADD=		-Wl,-E -Wl,-rpath,$(CC_OBJDIR) -L$(CC_OBJDIR) -lcc
.include <bsd.lib.mk>
