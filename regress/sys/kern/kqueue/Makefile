#	$OpenBSD: Makefile,v 1.3 2002/03/02 21:48:05 art Exp $

PROG=	kqueue-test
SRCS= kqueue-pipe.c kqueue-fork.c main.c kqueue-process.c

kq-pipe: ${PROG}
	./${PROG} -p
kq-fork: ${PROG}
	./${PROG} -f
kq-process: ${PROG}
	./${PROG} -P

REGRESSTARGETS=kq-pipe kq-fork kq-process
.PHONY: ${REGRESSTARGETS}

.include <bsd.regress.mk>
