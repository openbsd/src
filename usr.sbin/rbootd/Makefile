#	$OpenBSD: Makefile,v 1.4 2015/10/26 10:08:14 jung Exp $

PROG=	rbootd
SRCS=	bpf.c conf.c parseconf.c rbootd.c rmpproto.c utils.c
MAN=	rbootd.8
DPADD+=	${LIBUTIL}

.include <bsd.prog.mk>
