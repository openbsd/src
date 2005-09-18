# $OpenBSD: Makefile,v 1.3 2005/09/18 22:29:46 kurt Exp $

.include <../Makefile.inc>

LIB=bb
SRCS= bb.c
LDADD=		-Wl,-E -Wl,-rpath,$(CC_OBJDIR) -L$(CC_OBJDIR) -lcc
regress: all
.include <bsd.lib.mk>
