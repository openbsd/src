#	$OpenBSD: Makefile,v 1.7 2001/01/17 05:01:00 fgsch Exp $

PROG=	ipfstat
MAN=	ipfstat.8
SRCS=	fils.c parse.c opt.c kmem.c facpri.c common.c ifaddr.c
.PATH:  ${.CURDIR}/../../sbin/ipf
CFLAGS+=-I${.CURDIR}/../../sbin/ipf

.include <bsd.prog.mk>
