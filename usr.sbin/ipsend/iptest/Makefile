#	$Id: Makefile,v 1.1 1996/05/10 21:40:59 dm Exp $

PROG=	iptest
BINDIR=	/usr/sbin
NOMAN=		
SRCS=	iptest.c iptests.c ip.c sbpf.c sock.c 44arp.c
CFLAGS+=  -DDOSOCKET -I${.CURDIR}/../common -I${.CURDIR}/../../ipftest
.PATH:	${.CURDIR}/../common

.include <bsd.prog.mk>
