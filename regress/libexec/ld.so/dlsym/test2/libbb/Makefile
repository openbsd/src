# $OpenBSD: Makefile,v 1.4 2005/09/19 01:20:58 kurt Exp $

LIB=bb
SRCS= bb.c
LDADD=		-Wl,-E -Wl,-rpath,$(CC_OBJDIR) -L$(CC_OBJDIR) -lcc
regress: all
.include <bsd.lib.mk>
