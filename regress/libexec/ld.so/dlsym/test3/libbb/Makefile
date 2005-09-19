#	$OpenBSD: Makefile,v 1.1.1.1 2005/09/19 03:34:18 kurt Exp $

LIB=		bb
SRCS=		bb.c
LDADD+=		-Wl,-E
LDADD+=		-Wl,-rpath,$(CC_OBJDIR)
LDADD+=		-L$(CC_OBJDIR) -lcc

regress: all

.include <bsd.lib.mk>
