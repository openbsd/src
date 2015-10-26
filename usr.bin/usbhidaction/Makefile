#	$OpenBSD: Makefile,v 1.2 2015/10/26 10:08:14 jung Exp $
#	$NetBSD: Makefile,v 1.4 2002/02/02 16:54:26 veego Exp $

PROG=	usbhidaction
SRCS=	usbhidaction.c

LDADD+=	-lusbhid
DPADD+=	${LIBUSBHID} ${LIBUTIL}

.include <bsd.prog.mk>
