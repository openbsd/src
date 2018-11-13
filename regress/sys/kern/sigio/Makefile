#	$OpenBSD: Makefile,v 1.2 2018/11/13 13:05:42 visa Exp $

PROG=		sigio_test
SRCS=		main.c sigio_common.c sigio_pipe.c util.c

WARNINGS=	yes

TESTS+=		pipe_badpgid
TESTS+=		pipe_badsession
TESTS+=		pipe_cansigio
TESTS+=		pipe_getown
TESTS+=		pipe_read
#TESTS+=	pipe_write

TESTS_ROOT+=	pipe_cansigio

.for t in ${TESTS}
run-regress-${t}:
.if !empty(TESTS_ROOT:M${t})
	${SUDO} ./${PROG} ${t}
REGRESS_ROOT_TARGETS += run-regress-${t}
.else
	./${PROG} ${t}
.endif
REGRESS_TARGETS += run-regress-${t}
.endfor

.include <bsd.regress.mk>
