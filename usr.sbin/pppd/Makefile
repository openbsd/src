#	$OpenBSD: Makefile,v 1.15 2002/05/19 20:47:52 deraadt Exp $

PCAPDIR=${.CURDIR}/../../lib/libpcap

PROG=	pppd
SRCS=	auth.c cbcp.c ccp.c chap.c chap_ms.c demand.c fsm.c ipcp.c \
	ipxcp.c lcp.c magic.c main.c options.c sys-bsd.c upap.c

.PATH:	${.CURDIR}/../../lib/libpcap ${.CURDIR}/../../sys/net
MAN=	pppd.8
SUBDIR=	pppstats chat
BINMODE=4554
BINOWN=	root
BINGRP=	network

LDADD=	-lpcap -lutil
DPADD=	${LIBPCAP} ${LIBUTIL}
CFLAGS+= -I. -I${PCAPDIR} -DHAVE_PATHS_H -DIPX_CHANGE -DPPP_FILTER -DUSE_CRYPT -DCHAPMS

.include <bsd.prog.mk>
