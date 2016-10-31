#	$OpenBSD: Makefile,v 1.4 2016/10/31 23:45:09 tb Exp $

TEST_ENV=      "MALLOC_OPTIONS=FGJRX"

PROG=test_sshbuf
SRCS=tests.c
SRCS+=test_sshbuf.c
SRCS+=test_sshbuf_getput_basic.c
SRCS+=test_sshbuf_getput_crypto.c
SRCS+=test_sshbuf_misc.c
SRCS+=test_sshbuf_fuzz.c
SRCS+=test_sshbuf_getput_fuzz.c
SRCS+=test_sshbuf_fixed.c

run-regress-${PROG}: ${PROG}
	env ${TEST_ENV} ./${PROG}

.include <bsd.regress.mk>
