PROGS += test-http

SRCS_test-http=   test-http.c http.c io.c
LDADD_test-http+= -ltls -lssl -lcrypto -lutil
DPADD_test-http+= ${LIBTLS} ${LIBSSL} ${LIBCRYPTO} ${LIBUTIL}

run-regress-test-http: test-http
	./test-http https://jigsaw.w3.org/HTTP/TE/foo.txt foo.txt
	./test-http https://jigsaw.w3.org/HTTP/ChunkedScript chunk.out
	./test-http https://jigsaw.w3.org/HTTP/300/307.html redir.out
	sha256 -c ${.CURDIR}/test-http.sum


.PATH: ${.CURDIR}/..

.include <bsd.regress.mk>
