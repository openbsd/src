#	$OpenBSD: Makefile,v 1.9 2003/12/02 05:48:48 mickey Exp $

PROG=	kqueue-test
CFLAGS+=-Wall
SRCS=	kqueue-pipe.c kqueue-fork.c main.c kqueue-process.c kqueue-random.c \
	kqueue-pty.c kqueue-tun.c
LDADD=	-levent -lutil
DPADD=	${LIBEVENT} ${LIBUTIL}

kq-pipe: ${PROG}
	./${PROG} -p
kq-fork: ${PROG}
	./${PROG} -f
kq-process: ${PROG}
	./${PROG} -P
kq-random: ${PROG}
	./${PROG} -r
kq-tun: ${PROG}
	@EVENT_SHOW_METHOD=yes EVENT_NOKQUEUE=yes ${SUDO} ./${PROG} -t
	@EVENT_SHOW_METHOD=yes EVENT_NOSELECT=yes ${SUDO} ./${PROG} -t
	@${SUDO} ifconfig tun0 -alias down
	@${SUDO} ifconfig tun1 -alias down
kq-pty: ${PROG}
	${SUDO} ./${PROG} -T

REGRESS_TARGETS=kq-pipe kq-fork kq-process kq-random kq-tun kq-pty
.PHONY: ${REGRESS_TARGETS}

clean:
	@${SUDO} ifconfig tun0 -alias down
	@${SUDO} ifconfig tun1 -alias down

.include <bsd.regress.mk>
