#	$OpenBSD: Makefile,v 1.2 1998/01/26 04:17:08 dgregor Exp $

PROG=	iptest
BINDIR=	/usr/sbin
MAN=	iptest.1
SRCS=	iptest.c iptests.c ip.c sbpf.c sock.c 44arp.c
CFLAGS+=  -DDOSOCKET -I${.CURDIR}/../common -I${.CURDIR}/../../ipftest \
	-I${.CURDIR}/../../../sys/netinet -I${.CURDIR}/../../../sbin/ipf
.PATH:	${.CURDIR}/../common

.include <bsd.prog.mk>
