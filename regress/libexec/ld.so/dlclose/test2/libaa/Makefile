#	$OpenBSD: Makefile,v 1.1.1.1 2005/09/30 14:51:52 kurt Exp $

LIB=		aa
SRCS=		aa.c
LDADD+=		-Wl,-E
LDADD+=		-Wl,-rpath,$(BB_OBJDIR)
LDADD+=		-Wl,-rpath,$(CC_OBJDIR)
LDADD+=		-L$(BB_OBJDIR) -lbb
LDADD+=		-L$(CC_OBJDIR) -lcc

regress: all

.include <bsd.lib.mk>
