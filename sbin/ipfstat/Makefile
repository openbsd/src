#	$OpenBSD: Makefile,v 1.4 1998/09/15 09:58:33 pattonme Exp $

PROG=	ipfstat
MAN=	ipfstat.8
SRCS=	fils.c parse.c opt.c kmem.c 
.PATH:  ${.CURDIR}/../../sbin/ipf
CFLAGS+=-I${.CURDIR}/../../sbin/ipf



.include <bsd.prog.mk>
