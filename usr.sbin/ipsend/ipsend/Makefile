#	$Id: Makefile,v 1.1 1996/05/10 21:40:58 dm Exp $

PROG=	ipsend
BINDIR=	/usr/sbin
NOMAN=		
SRCS=	ipsend.c ip.c ipsopt.c sbpf.c sock.c 44arp.c
CFLAGS+=  -DDOSOCKET -I${.CURDIR}/../common -I${.CURDIR}/../../ipftest
.PATH:	${.CURDIR}/../common

.include <bsd.prog.mk>
