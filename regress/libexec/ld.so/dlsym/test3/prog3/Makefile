#	$OpenBSD: Makefile,v 1.1 2005/09/19 21:50:27 drahn Exp $

PROG=		prog3
SRCS=		main.c
CPPFLAGS+= 	-I$(AA_DIR)
LDADD+=		-laa
LDFLAGS+=	-Wl,-E
LDFLAGS+=	-Wl,-rpath,$(AA_OBJDIR)
LDFLAGS+=	-Wl,-rpath,$(BB_OBJDIR)
LDFLAGS+=	-Wl,-rpath,$(CC_OBJDIR)
LDFLAGS+=	-L$(AA_OBJDIR)

.include <bsd.regress.mk>
