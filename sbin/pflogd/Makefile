#	$OpenBSD: Makefile,v 1.9 2013/06/19 03:51:30 lteo Exp $

CFLAGS+=-Wall -Wmissing-prototypes -Wshadow

# for pcap-int.h
CFLAGS+=-I${.CURDIR}/../../lib/libpcap

LDADD+= -lpcap
DPADD+=	${LIBPCAP}

PROG=	pflogd
SRCS=	pflogd.c privsep.c privsep_fdpass.c
MAN=	pflogd.8

.include <bsd.prog.mk>
