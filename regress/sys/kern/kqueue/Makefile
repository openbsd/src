#	$OpenBSD: Makefile,v 1.4 2002/06/19 03:05:07 mickey Exp $

PROG=	kqueue-test
SRCS=	kqueue-pipe.c kqueue-fork.c main.c kqueue-process.c kqueue-random.c

kq-pipe: ${PROG}
	./${PROG} -p
kq-fork: ${PROG}
	./${PROG} -f
kq-process: ${PROG}
	./${PROG} -P
kq-random: ${PROG}
	./${PROG} -r

REGRESSTARGETS=kq-pipe kq-fork kq-process kq-random
.PHONY: ${REGRESSTARGETS}

.include <bsd.regress.mk>
