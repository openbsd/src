#	$OpenBSD: Makefile,v 1.1 2002/05/10 00:09:17 nate Exp $
#	$NetBSD: Makefile,v 1.4 2002/02/02 16:54:26 veego Exp $

PROG=	usbhidaction
SRCS=	usbhidaction.c

LDADD+=	-lusbhid -lutil
DPADD+=	${LIBUSBHID} ${LIBUTIL}

.include <bsd.prog.mk>
