#	$OpenBSD: Makefile,v 1.2 2001/12/30 07:10:42 pvalchev Exp $
#	$NetBSD: Makefile,v 1.4 1999/05/11 21:02:25 augustss Exp $

PROG=	usbhidctl
SRCS=	usbhid.c

LDADD+=	-lusbhid
DPADD+=	${LIBUSB}

.include <bsd.prog.mk>
