#	$OpenBSD: Makefile,v 1.5 1999/12/15 05:20:25 kjell Exp $

PROG=	ipfstat
MAN=	ipfstat.8
SRCS=	fils.c parse.c opt.c kmem.c facpri.c 
.PATH:  ${.CURDIR}/../../sbin/ipf
CFLAGS+=-I${.CURDIR}/../../sbin/ipf



.include <bsd.prog.mk>
