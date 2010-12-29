#	$OpenBSD: Makefile,v 1.3 2010/12/29 12:14:41 stsp Exp $
#	$NetBSD: Makefile,v 1.2 1997/06/12 16:18:50 mrg Exp $

PROG=	swapctl
SRCS=	swapctl.c swaplist.c
LDADD=	-lutil
DPADD=	${LIBUTIL}
MAN=	swapctl.8

LINKS=	${BINDIR}/swapctl ${BINDIR}/swapon
MLINKS=	swapctl.8 swapon.8

.include <bsd.prog.mk>
