#	$OpenBSD: Makefile,v 1.6 1999/12/15 05:20:24 kjell Exp $

PROG=	ipnat
MAN=	ipnat.8 ipnat.4 ipnat.5
SRCS=	ipnat.c kmem.c natparse.c
.PATH:  ${.CURDIR}/../ipfstat
CFLAGS+=-I${.CURDIR}/../../sbin/ipfstat

.include <bsd.prog.mk>
