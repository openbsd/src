#	$OpenBSD: Makefile,v 1.5 2002/09/02 20:01:44 avsm Exp $

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

REGRESS_TARGETS=kq-pipe kq-fork kq-process kq-random
.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>
