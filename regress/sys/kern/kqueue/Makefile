#	$OpenBSD: Makefile,v 1.7 2003/06/12 05:08:51 mickey Exp $

PROG=	kqueue-test
CFLAGS+=-Wall
SRCS=	kqueue-pipe.c kqueue-fork.c main.c kqueue-process.c kqueue-random.c \
	kqueue-pty.c
LDADD=	${LIBUTIL}
DPADD=	${DPUTIL}

kq-pipe: ${PROG}
	./${PROG} -p
kq-fork: ${PROG}
	./${PROG} -f
kq-process: ${PROG}
	./${PROG} -P
kq-random: ${PROG}
	./${PROG} -r
kq-pty: ${PROG}
	${SUDO} ./${PROG} -T

REGRESS_TARGETS=kq-pipe kq-fork kq-process kq-random kq-pty
.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>
