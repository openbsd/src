#	$OpenBSD: Makefile,v 1.4 1996/04/21 23:41:13 deraadt Exp $
#	$NetBSD: Makefile,v 1.12 1996/03/19 03:03:04 jtc Exp $

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
CFLAGS+= -I. -I${.CURDIR} -DHAVE_PATHS_H
CLEANFILES=y.tab.h grammar.c scanner.c

.include <bsd.prog.mk>
