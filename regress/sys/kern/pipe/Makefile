#	$OpenBSD: Makefile,v 1.2 2019/12/24 09:37:53 anton Exp $

PROG=		pipe

SRCS+=		pipe.c
SRCS+=		test-kqueue.c
SRCS+=		test-ping-pong.c
SRCS+=		test-run-down.c
SRCS+=		test-thundering-herd.c

LDADD+=		-lpthread
DPADD+=		${LIBPTHREAD}

WARNINGS=	yes

TESTS+=	kqueue-read
TESTS+=	kqueue-read-eof
TESTS+=	kqueue-write
TESTS+=	kqueue-write-eof
TESTS+=	ping-pong
TESTS+=	run-down-write-big
TESTS+=	run-down-write-small
TESTS+=	thundering-herd-read-signal
TESTS+=	thundering-herd-read-wakeup
TESTS+=	thundering-herd-write-signal
TESTS+=	thundering-herd-write-wakeup

REGRESS_SETUP_ONCE=	setup
setup: ${PROG}

.for t in ${TESTS}
${t}:
	@echo "\n======== ${@} ========"
	./${PROG} ${t}
REGRESS_TARGETS+=	${t}
.endfor

.include <bsd.regress.mk>
