#	$OpenBSD: Makefile,v 1.3 2015/10/26 10:32:56 tedu Exp $
#	$NetBSD: Makefile,v 1.4 2002/02/02 16:54:26 veego Exp $

PROG=	usbhidaction
SRCS=	usbhidaction.c

LDADD+=	-lusbhid
DPADD+=	${LIBUSBHID}

.include <bsd.prog.mk>
