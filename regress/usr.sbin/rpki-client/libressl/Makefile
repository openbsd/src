# $OpenBSD: Makefile,v 1.7 2022/10/14 17:17:13 tb Exp $

PROGS += test-http

SRCS_test-http=   test-http.c http.c io.c encoding.c
LDADD_test-http+= -ltls -lssl -lcrypto -lutil
DPADD_test-http+= ${LIBTLS} ${LIBSSL} ${LIBCRYPTO} ${LIBUTIL}

SERVER =	jigsaw.w3.org
REACHABLE =
.if make(run-regress-test-http)
REACHABLE !=	! ping -n -c 1 -w 1 ${SERVER} >/dev/null || echo yes
.endif

run-regress-test-http: test-http
.if ${REACHABLE:L} == "yes"
	./test-http https://${SERVER}/HTTP/TE/foo.txt foo.txt
	./test-http https://${SERVER}/HTTP/ChunkedScript chunk.out
	./test-http https://${SERVER}/HTTP/300/307.html redir.out
	sha256 -c ${.CURDIR}/test-http.sum
.else
	# server ${SERVER} not reachable
	@echo SKIPPED
.endif

REGRESS_TARGETS+=		run-regress-test-http-redirectchain
REGRESS_EXPECTED_FAILURES+=	run-regress-test-http-redirectchain
run-regress-test-http-redirectchain: test-http
	./test-http 'https://demo.cyotek.com/features/redirectchaintest.php?index=1' redirloop.out 2>&1 | grep -q 'Too many redirections requested'

.PATH: ${.CURDIR}/..

.include <bsd.regress.mk>
