#	$OpenBSD: Makefile,v 1.18 2014/03/21 19:55:11 naddy Exp $

PCAPDIR=${.CURDIR}/../../lib/libpcap

PROG=	pppd
SRCS=	auth.c cbcp.c ccp.c chap.c demand.c fsm.c ipcp.c \
	lcp.c magic.c main.c options.c sys-bsd.c upap.c

.PATH:	${.CURDIR}/../../lib/libpcap ${.CURDIR}/../../sys/net
MAN=	pppd.8
SUBDIR=	pppstats chat
BINMODE=4550
BINOWN=	root
BINGRP=	network

LDADD=	-lpcap -lutil
DPADD=	${LIBPCAP} ${LIBUTIL}
CFLAGS+= -I. -I${PCAPDIR} -DHAVE_PATHS_H -DPPP_FILTER

.include <bsd.prog.mk>
