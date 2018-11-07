# $OpenBSD: Makefile.inc,v 1.1.1.1 2018/11/07 01:08:49 bluhm Exp $

.PATH:			${.CURDIR}/..

SRCS_client =		client.c util.c
SRCS_server =		server.c util.c
WARNINGS =		yes
REGRESS_TARGETS	=

# check that program is linked with correct libraries

.for p in ${PROGS}
CLEANFILES +=		ldd-$p.out
REGRESS_TARGETS +=	run-ldd-$p
ldd-$p.out: $p
	LD_LIBRARY_PATH=${LD_LIBRARY_PATH} ldd $p >$@
.endfor

# run netcat server and connect with test client

CLEANFILES +=		client.out netcat-l.out netcat-l.fstat
REGRESS_TARGETS +=	run-client
run-client: client 127.0.0.1.crt
	@echo '\n======== $@ ========'
	echo "greeting" | nc -l -c -C 127.0.0.1.crt -K 127.0.0.1.key \
	    127.0.0.1 0 >netcat-l.out & \
	    sleep 1; fstat -p $$! >netcat-l.fstat
	LD_LIBRARY_PATH=${LD_LIBRARY_PATH} ./client \
	    `sed -n 's/.* stream tcp .*:/127.0.0.1 /p' netcat-l.fstat` \
	    >client.out
	# check that the client run successfully to the end
	grep -q '^success$$' client.out
	# client must have read server greeting
	grep -q '^<<< greeting$$' client.out
	# netstat server must have read client hello
	grep -q '^hello$$' netcat-l.out

# run test server and connect with netcat client

CLEANFILES +=		server.out netcat.out
REGRESS_TARGETS +=	run-server
run-server: server 127.0.0.1.crt
	@echo '\n======== $@ ========'
	LD_LIBRARY_PATH=${LD_LIBRARY_PATH} ./server 127.0.0.1 0 >server.out
	echo "hello" | nc -c -T noverify \
	    `sed -n 's/listen sock: //p' server.out` \
	    >netcat.out
	# check that the server child run successfully to the end
	grep -q '^success$$' server.out
	# server must have read client hello
	grep -q '^<<< hello$$' server.out
	# client must have read server greeting
	grep -q '^greeting$$' netcat.out

# create certificates for TLS

CLEANFILES +=		127.0.0.1.crt 127.0.0.1.key

127.0.0.1.crt:
	openssl req -batch -new \
	    -subj /L=OpenBSD/O=tls-regress/OU=server/CN=127.0.0.1/ \
	    -nodes -newkey rsa -keyout 127.0.0.1.key -x509 -out $@
