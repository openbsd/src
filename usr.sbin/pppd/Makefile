#	$OpenBSD: Makefile,v 1.10 1997/09/05 04:32:32 millert Exp $
#	$NetBSD: Makefile,v 1.13 1996/12/18 16:54:57 christos Exp $

PCAPDIR=${.CURDIR}/../../lib/libpcap

PROG=	pppd
SRCS=	auth.c cbcp.c ccp.c chap.c chap_ms.c demand.c fsm.c ipcp.c \
	ipxcp.c lcp.c magic.c main.c options.c sys-bsd.c upap.c

.PATH:	${.CURDIR}/../../lib/libpcap ${.CURDIR}/../../sys/net
MAN=	pppd.8
SUBDIR=	pppstats chat
BINMODE=4555
BINOWN=	root

LDADD=	-lpcap -lutil
DPADD=	${LIBPCAP} ${LIBUTIL}
CFLAGS+= -I. -I${PCAPDIR} -DHAVE_PATHS_H -DIPX_CHANGE -DPPP_FILTER -DUSE_CRYPT

MLINKS=	pppd.8 ppp.8

.include <bsd.prog.mk>
