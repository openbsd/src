#	$OpenBSD: Makefile,v 1.2 2002/02/27 17:11:51 art Exp $

PROG=	kqueue-test
SRCS= kqueue-pipe.c kqueue-fork.c main.c

kq-pipe: ${PROG}
	./${PROG} -p
kq-fork: ${PROG}
	./${PROG} -f

REGRESSTARGETS=kq-pipe kq-fork
.PHONY: ${REGRESSTARGETS}

.include <bsd.regress.mk>
