#	$OpenBSD: Makefile,v 1.3 2001/11/18 23:39:18 deraadt Exp $

PROG =	rpc.rusersd
SRCS =	rusersd.c rusers_proc.c
MAN =	rpc.rusersd.8
MLINKS=	rpc.rusersd.8 rusersd.8

DPADD=	${LIBRPCSVC}
LDADD=	-lrpcsvc

.include <bsd.prog.mk>

