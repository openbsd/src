#	$OpenBSD: Makefile,v 1.8 2003/06/15 16:41:34 mickey Exp $

PROG=	kqueue-test
CFLAGS+=-Wall
SRCS=	kqueue-pipe.c kqueue-fork.c main.c kqueue-process.c kqueue-random.c \
	kqueue-pty.c
LDADD=	-lutil
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
