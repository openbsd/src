#	$OpenBSD: Makefile,v 1.9 2000/04/26 21:04:00 deraadt Exp $

PROG=	ipnat
MAN=	ipnat.8 ipnat.4 ipnat.5
SRCS=	ipnat.c kmem.c natparse.c ifaddr.c
.PATH:  ${.CURDIR}/../ipfstat ${.CURDIR}/../ipf
CFLAGS+=-I${.CURDIR}/../../sbin/ipfstat -I${.CURDIR}/../ipf

.include <bsd.prog.mk>
