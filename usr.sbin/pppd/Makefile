#	$OpenBSD: Makefile,v 1.3 1996/03/25 15:55:29 niklas Exp $

PROG=	pppd
SRCS=	main.c magic.c fsm.c lcp.c ipcp.c upap.c chap.c md5.c ccp.c \
	auth.c options.c sys-bsd.c demand.c gencode.c grammar.c scanner.c \
	nametoaddr.c optimize.c bpf_filter.c
.PATH:	${.CURDIR}/../../lib/libpcap ${.CURDIR}/../../sys/net
MAN=	pppd.8
SUBDIR=	pppstats chat
BINMODE=4555
BINOWN=	root

#LDADD=	-lcrypt -lutil
LDADD=	-lutil
#DPADD=	${LIBCRYPT} ${LIBUTIL}
DPADD=	${LIBUTIL}
CFLAGS+= -DHAVE_PATHS_H

.include <bsd.prog.mk>
