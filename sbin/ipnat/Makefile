#	$OpenBSD: Makefile,v 1.10 2001/01/17 05:01:01 fgsch Exp $

PROG=	ipnat
MAN=	ipnat.8 ipnat.4 ipnat.5
SRCS=	ipnat.c kmem.c natparse.c common.c ifaddr.c
.PATH:  ${.CURDIR}/../ipfstat ${.CURDIR}/../ipf
CFLAGS+=-I${.CURDIR}/../../sbin/ipfstat -I${.CURDIR}/../ipf

.include <bsd.prog.mk>
