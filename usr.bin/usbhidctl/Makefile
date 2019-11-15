#	$OpenBSD: Makefile,v 1.3 2019/11/15 00:06:46 jsg Exp $
#	$NetBSD: Makefile,v 1.4 1999/05/11 21:02:25 augustss Exp $

PROG=	usbhidctl
SRCS=	usbhid.c

LDADD+=	-lusbhid
DPADD+=	${LIBUSBHID}

.include <bsd.prog.mk>
