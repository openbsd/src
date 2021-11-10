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
	./test-http 'https://demo.cyotek.com/features/redirectchaintest.php?index=1' redirloop.out 2>&1 | grep -q 'Too many redirections requested'
.else
	# server ${SERVER} not reachable
	@echo SKIPPED
.endif

.PATH: ${.CURDIR}/..

.include <bsd.regress.mk>
