#	$OpenBSD: Makefile,v 1.5 1999/02/07 00:48:29 deraadt Exp $

PROG=	ipnat
MAN=	ipnat.8 ipnat.4 ipnat.5
SRCS=	ipnat.c kmem.c 
.PATH:  ${.CURDIR}/../ipfstat
CFLAGS+=-I${.CURDIR}/../../sbin/ipfstat

.include <bsd.prog.mk>
