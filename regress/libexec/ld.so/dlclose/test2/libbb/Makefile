#	$OpenBSD: Makefile,v 1.1.1.1 2005/09/30 14:51:52 kurt Exp $

LIB=		bb
SRCS=		bb.c
LDADD+=		-Wl,-E
LDADD+=		-Wl,-rpath,$(ZZ_OBJDIR)
LDADD+=		-L$(ZZ_OBJDIR) -lzz

regress: all

.include <bsd.lib.mk>
