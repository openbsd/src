#	$OpenBSD: Makefile,v 1.3 1998/01/26 19:46:23 weingart Exp $

PROG=	ipsend
BINDIR=	/usr/sbin
MAN=    ipsend.1 ipsend.5
SRCS=	ipsend.c ip.c ipsopt.c sbpf.c sock.c 44arp.c iplang_y.y iplang_l.l
CFLAGS+=  -DDOSOCKET -I${.CURDIR}/../common -I${.CURDIR}/../../ipftest \
	-I${.CURDIR}/../../../sbin/ipf -I${.CURDIR}/../../../sys/netinet \
	-I${.OBJDIR}

LDADD   = -lfl
CLEANFILES+=y.tab.h

.PATH:	${.CURDIR}/../common

.include <bsd.prog.mk>
