#	$OpenBSD: Makefile,v 1.1 2001/09/02 17:55:29 pvalchev Exp $
#	$NetBSD: Makefile,v 1.4 1999/05/11 21:02:25 augustss Exp $

PROG=	usbhidctl
SRCS=	usbhid.c

LDADD+=	-lusb
DPADD+=	${LIBUSB}

.include <bsd.prog.mk>
