#	$OpenBSD: Makefile,v 1.7 1999/12/16 07:38:45 kjell Exp $

PROG=	ipnat
MAN=	ipnat.8 ipnat.4 ipnat.5
SRCS=	ipnat.c kmem.c natparse.c ifaddr.c
.PATH:  ${.CURDIR}/../ipfstat ${.CURDIR}/../ipf
CFLAGS+=-I${.CURDIR}/../../sbin/ipfstat -I${.CURDIR}/../ipf

.include <bsd.prog.mk>
