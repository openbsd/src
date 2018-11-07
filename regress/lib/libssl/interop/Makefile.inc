# $OpenBSD: Makefile.inc,v 1.3 2018/11/07 20:46:28 bluhm Exp $

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

CLEANFILES +=		nc-client.out netcat-l.out netcat-l.fstat
REGRESS_TARGETS +=	run-client
nc-client.out run-client: client 127.0.0.1.crt
	@echo '\n======== $@ ========'
	echo "greeting" | nc -l -c -C 127.0.0.1.crt -K 127.0.0.1.key \
	    127.0.0.1 0 >netcat-l.out & \
	    sleep 1; fstat -p $$! >netcat-l.fstat
	LD_LIBRARY_PATH=${LD_LIBRARY_PATH} ./client \
	    `sed -n 's/.* stream tcp .*:/127.0.0.1 /p' netcat-l.fstat` \
	    >nc-client.out
	# check that the client run successfully to the end
	grep -q '^success$$' nc-client.out
	# client must have read server greeting
	grep -q '^<<< greeting$$' nc-client.out
	# netstat server must have read client hello
	grep -q '^hello$$' netcat-l.out

# run test server and connect with netcat client

CLEANFILES +=		nc-server.out netcat.out
REGRESS_TARGETS +=	run-server
nc-server.out run-server: server 127.0.0.1.crt
	@echo '\n======== $@ ========'
	LD_LIBRARY_PATH=${LD_LIBRARY_PATH} ./server 127.0.0.1 0 \
	    >nc-server.out
	echo "hello" | nc -c -T noverify \
	    `sed -n 's/listen sock: //p' nc-server.out` \
	    >netcat.out
	# check that the server child run successfully to the end
	grep -q '^success$$' nc-server.out
	# server must have read client hello
	grep -q '^<<< hello$$' nc-server.out
	# client must have read server greeting
	grep -q '^greeting$$' netcat.out

# run test server and with test client, self test the ssl library

CLEANFILES +=		self-client.out self-server.out
REGRESS_TARGETS +=	run-self
self-client.out self-server.out run-self: client server 127.0.0.1.crt
	@echo '\n======== $@ ========'
	LD_LIBRARY_PATH=${LD_LIBRARY_PATH} ./server 127.0.0.1 0 \
	    >self-server.out
	LD_LIBRARY_PATH=${LD_LIBRARY_PATH} ./client \
	    `sed -n 's/listen sock: //p' self-server.out` \
	    >self-client.out
	# check that the client run successfully to the end
	grep -q '^success$$' self-client.out
	# client must have read server greeting
	grep -q '^<<< greeting$$' self-client.out
	# check that the server child run successfully to the end
	grep -q '^success$$' self-server.out
	# server must have read client hello
	grep -q '^<<< hello$$' self-server.out

.for o in nc-client nc-server self-client self-server

# check that client and server have used correct runtime library

REGRESS_TARGETS +=	run-version-$o

# check that client and server have used correct TLS protocol

REGRESS_TARGETS +=	run-protocol-$o

.endfor

# create certificates for TLS

CLEANFILES +=		127.0.0.1.crt 127.0.0.1.key

127.0.0.1.crt:
	openssl req -batch -new \
	    -subj /L=OpenBSD/O=tls-regress/OU=server/CN=127.0.0.1/ \
	    -nodes -newkey rsa -keyout 127.0.0.1.key -x509 -out $@
