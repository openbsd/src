#	$OpenBSD: Makefile,v 1.6 1999/12/16 07:38:45 kjell Exp $

PROG=	ipfstat
MAN=	ipfstat.8
SRCS=	fils.c parse.c opt.c kmem.c facpri.c ifaddr.c
.PATH:  ${.CURDIR}/../../sbin/ipf
CFLAGS+=-I${.CURDIR}/../../sbin/ipf



.include <bsd.prog.mk>
