# $OpenBSD: Makefile,v 1.10 2023/06/13 11:41:36 claudio Exp $

PROGS +=	test-http

SRCS_test-http =	test-http.c http.c io.c encoding.c
LDADD_test-http =	-ltls -lssl -lcrypto -lutil -lz
DPADD_test-http =	${LIBTLS} ${LIBSSL} ${LIBCRYPTO} ${LIBUTIL} ${LIBZ}

SERVER =	jigsaw.w3.org
.if ! (make(clean) || make(cleandir) || make(obj))
REACHABLE !?=	! ping -n -c 1 -w 1 ${SERVER} >/dev/null || echo yes
.MAKEFLAGS :=	REACHABLE=${REACHABLE}
.endif

run-regress-test-http: test-http
.if "${REACHABLE:L}" == "yes"
	./test-http https://${SERVER}/HTTP/TE/foo.txt foo.txt
	./test-http https://${SERVER}/HTTP/ChunkedScript chunk.out
	./test-http https://${SERVER}/HTTP/300/307.html redir.out
	sha256 -c ${.CURDIR}/test-http.sum
.else
	# server ${SERVER} not reachable
	@echo SKIPPED
.endif

REGRESS_TARGETS+=		run-regress-test-http-redirectchain
run-regress-test-http-redirectchain: test-http
.if "${REACHABLE:L}" == "yes"
	./test-http \
	    'https://demo.cyotek.com/features/redirectchaintest.php?index=1' \
	    redirloop.out 2>&1 | tee redirloop.err
	grep -q 'Too many redirections requested' redirloop.err
.else
	# server ${SERVER} not reachable
	@echo SKIPPED
.endif

.PATH: ${.CURDIR}/..

.include <bsd.regress.mk>
