#	$OpenBSD: Makefile,v 1.8 2000/04/26 15:45:27 espie Exp $

PROG=	ipnat
MAN=	ipnat.8 ipnat.4 ipnat.5
SRCS=	ipnat.c kmem.c natparse.c ifaddr.c
.PATH:  ${.CURDIR}/../ipfstat ${.CURDIR}/../ipf
CFLAGS+=-I${.CURDIR}/../../sbin/ipfstat -I${.CURDIR}/../ipf
MLINKS=	ipnat.5 ipnat.conf.5

.include <bsd.prog.mk>
