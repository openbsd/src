#	$OpenBSD: Makefile,v 1.4 1998/09/15 09:57:29 pattonme Exp $

PROG=	ipnat
MAN=	ipnat.1 ipnat.4 ipnat.5
SRCS=	ipnat.c kmem.c 
.PATH:  ${.CURDIR}/../ipfstat
CFLAGS+=-I${.CURDIR}/../../sbin/ipfstat

.include <bsd.prog.mk>
