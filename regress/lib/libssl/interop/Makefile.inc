# $OpenBSD: Makefile.inc,v 1.4 2018/11/09 06:30:41 bluhm Exp $

.PATH:			${.CURDIR}/..

SRCS_client =		client.c util.c
SRCS_server =		server.c util.c
WARNINGS =		yes

# check that program is linked with correct libraries

.for p in ${PROGS}
CLEANFILES +=		ldd-$p.out
ldd-$p.out: $p
	LD_LIBRARY_PATH=${LD_LIBRARY_PATH} ldd $p >$@
.endfor

# run netcat server and connect with test client

CLEANFILES +=		nc-client.out netcat-l.out netcat-l.fstat
nc-client.out run-client: client 127.0.0.1.crt
	@echo '\n======== $@ ========'
	echo "greeting" | nc -l -c -C 127.0.0.1.crt -K 127.0.0.1.key \
	    127.0.0.1 0 >netcat-l.out & \
	    for i in `jot 1000`; do fstat -p $$! >netcat-l.fstat; \
	    grep -q ' stream tcp .*:[1-9][0-9]*$$' netcat-l.fstat && exit 0; \
	    done; exit 1
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
nc-server.out run-server: server 127.0.0.1.crt
	@echo '\n======== $@ ========'
	LD_LIBRARY_PATH=${LD_LIBRARY_PATH} ./server 127.0.0.1 0 \
	    >nc-server.out
	echo "hello" | nc -c -R 127.0.0.1.crt \
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

# create certificates for TLS

CLEANFILES +=		127.0.0.1.{crt,key} \
			ca.{crt,key,srl} fake-ca.{crt,key} \
			{client,server}.{req,crt,key}

127.0.0.1.crt:
	openssl req -batch -new \
	    -subj /L=OpenBSD/O=tls-regress/OU=server/CN=127.0.0.1/ \
	    -nodes -newkey rsa -keyout 127.0.0.1.key -x509 -out $@

ca.crt fake-ca.crt:
	openssl req -batch -new \
	    -subj /L=OpenBSD/O=tls-regress/OU=ca/CN=root/ \
	    -nodes -newkey rsa -keyout ${@:R}.key -x509 -out $@

client.req server.req:
	openssl req -batch -new \
	    -subj /L=OpenBSD/O=tls-regress/OU=${@:R}/CN=localhost/ \
	    -nodes -newkey rsa -keyout ${@:R}.key -out $@

client.crt server.crt: ca.crt ${@:R}.req
	openssl x509 -CAcreateserial -CAkey ca.key -CA ca.crt \
	    -req -in ${@:R}.req -out $@
