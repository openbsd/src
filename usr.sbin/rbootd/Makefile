#	$OpenBSD: Makefile,v 1.3 2001/12/01 23:27:23 miod Exp $

PROG=	rbootd
SRCS=	bpf.c conf.c parseconf.c rbootd.c rmpproto.c utils.c
MAN=	rbootd.8
LDADD+=	-lutil
DPADD+=	${LIBUTIL}

.include <bsd.prog.mk>
