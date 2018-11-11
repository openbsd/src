# $OpenBSD: Makefile.inc,v 1.5 2018/11/11 00:15:04 bluhm Exp $

.PATH:			${.CURDIR}/..

SRCS_client =		client.c util.c
SRCS_server =		server.c util.c
WARNINGS =		yes
CLEANFILES +=		*.out *.fstat

.for p in ${PROGS}
ldd-$p.out: $p
	# programs must be linked with correct libraries
	LD_LIBRARY_PATH=${LD_LIBRARY_PATH} ldd $p >$@
.endfor

client-self.out server-self.out: run-self-client-server

run-self-client-server: client server 127.0.0.1.crt
	@echo '\n======== $@ ========'
	# check that tls client and server work together
	LD_LIBRARY_PATH=${LD_LIBRARY_PATH} \
	    ./server >server-self.out \
	    127.0.0.1 0
	LD_LIBRARY_PATH=${LD_LIBRARY_PATH} \
	    ./client >client-self.out \
	    `sed -n 's/listen sock: //p' server-self.out`
	# check that the client run successfully to the end
	grep -q '^success$$' client-self.out
	# client must have read server greeting
	grep -q '^<<< greeting$$' client-self.out
	# check that the server child run successfully to the end
	grep -q '^success$$' server-self.out
	# server must have read client hello
	grep -q '^<<< hello$$' server-self.out

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
