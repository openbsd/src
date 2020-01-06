#	$OpenBSD: Makefile,v 1.1.1.1 2020/01/06 22:36:57 bluhm Exp $

# Copyright (c) 2020 Alexander Bluhm <bluhm@openbsd.org>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

NC =			./netcat-regress

CLEANFILES =		${NC:T} {client,server}.{out,err,port,sock} ktrace.out

REGRESS_SETUP =		setup
setup:
	@echo '======== $@ ========'
	pkill ${NC:T} || true
	rm -f ${NC:T}
	cp /usr/bin/nc ${NC:T}
	chmod 755 ${NC:T}

REGRESS_CLEANUP =	cleanup
cleanup:
	@echo '======== $@ ========'
	-pkill ${NC:T} || true

REGRESS_TARGETS =

SERVER_NC = echo greeting | ${NC}
CLIENT_NC = echo command | ${NC}
SERVER_BG = 2>&1 >server.out | tee server.err &
CLIENT_BG = 2>&1 >client.out | tee client.err &
SERVER_LOG = >server.out 2>server.err
CLIENT_LOG = >client.out 2>client.err

PORT_GET = \
	sed -E -n 's/(Listening|Bound) on .* //p' server.err >server.port
PORT = `cat server.port`

LISTEN_WAIT = \
	let timeout=`date +%s`+5; \
	until grep -q 'Listening on ' server.err; \
	do [[ `date +%s` -lt $$timeout ]] || exit 1; done

BIND_WAIT = \
	let timeout=`date +%s`+5; \
	until grep -q 'Bound on ' server.err; \
	do [[ `date +%s` -lt $$timeout ]] || exit 1; done

CONNECT_WAIT = \
	let timeout=`date +%s`+5; \
	until grep -q 'Connection to ' client.err; \
	do [[ `date +%s` -lt $$timeout ]] || exit 1; done

TRANSFER_WAIT = \
	let timeout=`date +%s`+5; \
	until grep -q 'greeting' client.out && grep -q 'command' server.out; \
	do [[ `date +%s` -lt $$timeout ]] || exit 1; done

### TCP ####

REGRESS_TARGETS +=	run-tcp
run-tcp:
	@echo '======== $@ ========'
	${SERVER_NC} -n -v -l 127.0.0.1 0 ${SERVER_BG}
	${LISTEN_WAIT}
	${PORT_GET}
	${CLIENT_NC} -n -v 127.0.0.1 ${PORT} ${CLIENT_BG}
	${CONNECT_WAIT}
	${TRANSFER_WAIT}
	grep '^greeting$$' client.out
	grep '^command$$' server.out
	grep 'Listening on 127.0.0.1 ' server.err
	grep 'Connection received on 127.0.0.1 ' server.err
	grep 'Connection to 127.0.0.1 .* succeeded!' client.err

REGRESS_TARGETS +=	run-tcp6
run-tcp6:
	@echo '======== $@ ========'
	${SERVER_NC} -n -v -l ::1 0 ${SERVER_BG}
	${LISTEN_WAIT}
	${PORT_GET}
	${CLIENT_NC} -n -v ::1 ${PORT} ${CLIENT_BG}
	${CONNECT_WAIT}
	${TRANSFER_WAIT}
	grep '^greeting$$' client.out
	grep '^command$$' server.out
	grep 'Listening on ::1 ' server.err
	grep 'Connection received on ::1 ' server.err
	grep 'Connection to ::1 .* succeeded!' client.err

# TCP resolver

REGRESS_TARGETS +=	run-tcp-localhost-server
run-tcp-localhost-server:
	@echo '======== $@ ========'
	${SERVER_NC} -4 -v -l localhost 0 ${SERVER_BG}
	${LISTEN_WAIT}
	${PORT_GET}
	${CLIENT_NC} -n -v 127.0.0.1 ${PORT} ${CLIENT_BG}
	${CONNECT_WAIT}
	${TRANSFER_WAIT}
	grep '^greeting$$' client.out
	grep '^command$$' server.out
	grep 'Listening on localhost ' server.err
	grep 'Connection received on localhost ' server.err
	grep 'Connection to 127.0.0.1 .* succeeded!' client.err

REGRESS_TARGETS +=	run-tcp6-localhost-server
run-tcp6-localhost-server:
	@echo '======== $@ ========'
	${SERVER_NC} -6 -v -l localhost 0 ${SERVER_BG}
	${LISTEN_WAIT}
	${PORT_GET}
	${CLIENT_NC} -n -v ::1 ${PORT} ${CLIENT_BG}
	${CONNECT_WAIT}
	${TRANSFER_WAIT}
	grep '^greeting$$' client.out
	grep '^command$$' server.out
	grep 'Listening on localhost ' server.err
	grep 'Connection received on localhost ' server.err
	grep 'Connection to ::1 .* succeeded!' client.err

REGRESS_TARGETS +=	run-tcp-localhost-client
run-tcp-localhost-client:
	@echo '======== $@ ========'
	${SERVER_NC} -n -v -l 127.0.0.1 0 ${SERVER_BG}
	${LISTEN_WAIT}
	${PORT_GET}
	${CLIENT_NC} -4 -v localhost ${PORT} ${CLIENT_BG}
	${CONNECT_WAIT}
	${TRANSFER_WAIT}
	grep '^greeting$$' client.out
	grep '^command$$' server.out
	grep 'Listening on 127.0.0.1 ' server.err
	grep 'Connection received on 127.0.0.1 ' server.err
	grep 'Connection to localhost .* succeeded!' client.err

REGRESS_TARGETS +=	run-tcp6-localhost-client
run-tcp6-localhost-client:
	@echo '======== $@ ========'
	${SERVER_NC} -n -v -l ::1 0 ${SERVER_BG}
	${LISTEN_WAIT}
	${PORT_GET}
	${CLIENT_NC} -6 -v localhost ${PORT} ${CLIENT_BG}
	${CONNECT_WAIT}
	${TRANSFER_WAIT}
	grep '^greeting$$' client.out
	grep '^command$$' server.out
	grep 'Listening on ::1 ' server.err
	grep 'Connection received on ::1 ' server.err
	grep 'Connection to localhost .* succeeded!' client.err

REGRESS_TARGETS +=	run-tcp-bad-localhost-server
run-tcp-bad-localhost-server:
	@echo '======== $@ ========'
	! ${NC} -4 -v -l ::1 0 ${SERVER_LOG}
	grep 'no address associated with name' server.err

REGRESS_TARGETS +=	run-tcp6-bad-localhost-server
run-tcp6-bad-localhost-server:
	@echo '======== $@ ========'
	! ${NC} -6 -v -l 127.0.0.0 0 ${SERVER_LOG}
	grep 'no address associated with name' server.err

REGRESS_TARGETS +=	run-tcp-bad-localhost-client
run-tcp-bad-localhost-client:
	@echo '======== $@ ========'
	${SERVER_NC} -n -v -l 127.0.0.1 0 ${SERVER_BG}
	${LISTEN_WAIT}
	${PORT_GET}
	! ${NC} -4 -v ::1 ${PORT} ${CLIENT_LOG}
	grep 'no address associated with name' client.err

REGRESS_TARGETS +=	run-tcp6-bad-localhost-client
run-tcp6-bad-localhost-client:
	@echo '======== $@ ========'
	${SERVER_NC} -n -v -l 127.0.0.1 0 ${SERVER_BG}
	${LISTEN_WAIT}
	${PORT_GET}
	! ${NC} -6 -v 127.0.0.1 ${PORT} ${CLIENT_LOG}
	grep 'no address associated with name' client.err

### TLS ###

REGRESS_TARGETS +=	run-tls
run-tls: 127.0.0.1.crt
	@echo '======== $@ ========'
	${SERVER_NC} -c -C 127.0.0.1.crt -K 127.0.0.1.key -n -v -l 127.0.0.1 0 \
	    ${SERVER_BG}
	${LISTEN_WAIT}
	${PORT_GET}
	${CLIENT_NC} -c -R 127.0.0.1.crt -n -v 127.0.0.1 ${PORT} ${CLIENT_BG}
	${CONNECT_WAIT}
	${TRANSFER_WAIT}
	grep '^greeting$$' client.out
	grep '^command$$' server.out
	grep 'Listening on 127.0.0.1 ' server.err
	grep 'Connection received on 127.0.0.1 ' server.err
	grep 'Connection to 127.0.0.1 .* succeeded!' client.err
	grep 'Subject: .*/OU=server/CN=127.0.0.1' client.err
	grep 'Issuer: .*/OU=server/CN=127.0.0.1' client.err

REGRESS_TARGETS +=	run-tls6
run-tls6: 1.crt
	@echo '======== $@ ========'
	${SERVER_NC} -c -C 1.crt -K 1.key -n -v -l ::1 0 ${SERVER_BG}
	${LISTEN_WAIT}
	${PORT_GET}
	${CLIENT_NC} -c -R 1.crt -n -v ::1 ${PORT} ${CLIENT_BG}
	${CONNECT_WAIT}
	${TRANSFER_WAIT}
	grep '^greeting$$' client.out
	grep '^command$$' server.out
	grep 'Listening on ::1 ' server.err
	grep 'Connection received on ::1 ' server.err
	grep 'Connection to ::1 .* succeeded!' client.err
	grep 'Subject: .*/OU=server/CN=::1' client.err
	grep 'Issuer: .*/OU=server/CN=::1' client.err

REGRESS_TARGETS +=	run-tls-localhost
run-tls-localhost: server.crt ca.crt
	@echo '======== $@ ========'
	${SERVER_NC} -c -C server.crt -K server.key -v -l localhost 0 \
	    ${SERVER_BG}
	${LISTEN_WAIT}
	${PORT_GET}
	${CLIENT_NC} -c -R ca.crt -v localhost ${PORT} ${CLIENT_BG}
	${CONNECT_WAIT}
	${TRANSFER_WAIT}
	grep '^greeting$$' client.out
	grep '^command$$' server.out
	grep 'Listening on localhost ' server.err
	grep 'Connection received on localhost ' server.err
	grep 'Connection to localhost .* succeeded!' client.err
	grep 'Subject: .*/OU=server/CN=localhost' client.err
	grep 'Issuer: .*/OU=ca/CN=root' client.err

REGRESS_TARGETS +=	run-tls-bad-ca
run-tls-bad-ca: server.crt fake-ca.crt
	@echo '======== $@ ========'
	${SERVER_NC} -c -C server.crt -K server.key -v -l localhost 0 \
	    ${SERVER_BG}
	${LISTEN_WAIT}
	${PORT_GET}
	! ${NC} -c -R fake-ca.crt -v localhost ${PORT} ${CLIENT_LOG}
	${CONNECT_WAIT}
	grep 'Listening on localhost ' server.err
	grep 'Connection received on localhost ' server.err
	grep 'certificate signature failure' client.err

REGRESS_TARGETS +=	run-tls-name
run-tls-name: server.crt ca.crt
	@echo '======== $@ ========'
	${SERVER_NC} -c -C server.crt -K server.key -n -v -l 127.0.0.1 0 \
	    ${SERVER_BG}
	${LISTEN_WAIT}
	${PORT_GET}
	${CLIENT_NC} -c -e localhost -R ca.crt -n -v 127.0.0.1 ${PORT} \
	    ${CLIENT_BG}
	${CONNECT_WAIT}
	${TRANSFER_WAIT}
	grep '^greeting$$' client.out
	grep '^command$$' server.out
	grep 'Listening on 127.0.0.1 ' server.err
	grep 'Connection received on 127.0.0.1 ' server.err
	grep 'Connection to 127.0.0.1 .* succeeded!' client.err
	grep 'Subject: .*/OU=server/CN=localhost' client.err
	grep 'Issuer: .*/OU=ca/CN=root' client.err

REGRESS_TARGETS +=	run-tls-hash
run-tls-hash: server.crt server.hash ca.crt
	@echo '======== $@ ========'
	${SERVER_NC} -c -C server.crt -K server.key -v -l localhost 0 \
	    ${SERVER_BG}
	${LISTEN_WAIT}
	${PORT_GET}
	${CLIENT_NC} -c -R ca.crt -H `cat server.hash` -v localhost ${PORT} \
	    ${CLIENT_BG}
	${CONNECT_WAIT}
	${TRANSFER_WAIT}
	grep '^greeting$$' client.out
	grep '^command$$' server.out
	grep 'Listening on localhost ' server.err
	grep 'Connection received on localhost ' server.err
	grep 'Connection to localhost .* succeeded!' client.err
	grep 'Subject: .*/OU=server/CN=localhost' client.err
	grep 'Issuer: .*/OU=ca/CN=root' client.err
	grep 'Cert Hash: SHA256:' client.err

### UDP ####

REGRESS_TARGETS +=	run-udp
run-udp:
	@echo '======== $@ ========'
	${SERVER_NC} -u -n -v -l 127.0.0.1 0 ${SERVER_BG}
	${BIND_WAIT}
	${PORT_GET}
	# the -v option would cause udptest() to write additional X
	${CLIENT_NC} -u -n 127.0.0.1 ${PORT} ${CLIENT_BG}
	${TRANSFER_WAIT}
	grep '^greeting$$' client.out
	grep '^command$$' server.out
	grep 'Bound on 127.0.0.1 ' server.err
	grep 'Connection received on 127.0.0.1 ' server.err

REGRESS_TARGETS +=	run-udp6
run-udp6:
	@echo '======== $@ ========'
	${SERVER_NC} -u -n -v -l ::1 0 ${SERVER_BG}
	${BIND_WAIT}
	${PORT_GET}
	# the -v option would cause udptest() to write additional X
	${CLIENT_NC} -u -n ::1 ${PORT} ${CLIENT_BG}
	${TRANSFER_WAIT}
	grep '^greeting$$' client.out
	grep '^command$$' server.out
	grep 'Bound on ::1 ' server.err
	grep 'Connection received on ::1 ' server.err

REGRESS_TARGETS +=	run-udp-udptest
run-udp-udptest:
	@echo '======== $@ ========'
	${SERVER_NC} -u -n -v -l 127.0.0.1 0 ${SERVER_BG}
	${BIND_WAIT}
	${PORT_GET}
	${CLIENT_NC} -u -v -n 127.0.0.1 ${PORT} ${CLIENT_BG}
	${TRANSFER_WAIT}
	grep '^greeting$$' client.out
	# client sends 4 X UDP packets to check connection
	grep '^XXXXcommand$$' server.out
	grep 'Bound on 127.0.0.1 ' server.err
	grep 'Connection received on 127.0.0.1 ' server.err
	grep 'Connection to 127.0.0.1 .* succeeded!' client.err

# UDP resolver

REGRESS_TARGETS +=	run-udp-localhost
run-udp-localhost:
	@echo '======== $@ ========'
	${SERVER_NC} -u -4 -v -l localhost 0 ${SERVER_BG}
	${BIND_WAIT}
	${PORT_GET}
	# the -v option would cause udptest() to write additional X
	${CLIENT_NC} -u -4 localhost ${PORT} ${CLIENT_BG}
	${TRANSFER_WAIT}
	grep '^greeting$$' client.out
	grep '^command$$' server.out
	grep 'Bound on localhost ' server.err
	grep 'Connection received on localhost ' server.err

REGRESS_TARGETS +=	run-udp6-localhost
run-udp6-localhost:
	@echo '======== $@ ========'
	${SERVER_NC} -u -6 -v -l localhost 0 ${SERVER_BG}
	${BIND_WAIT}
	${PORT_GET}
	# the -v option would cause udptest() to write additional X
	${CLIENT_NC} -u -6 localhost ${PORT} ${CLIENT_BG}
	${TRANSFER_WAIT}
	grep '^greeting$$' client.out
	grep '^command$$' server.out
	grep 'Bound on localhost ' server.err
	grep 'Connection received on localhost ' server.err

### UNIX ####

REGRESS_TARGETS +=	run-unix
run-unix:
	@echo '======== $@ ========'
	rm -f server.sock
	${SERVER_NC} -U -n -v -l server.sock ${SERVER_BG}
	${LISTEN_WAIT}
	${CLIENT_NC} -U -n -v server.sock ${CLIENT_BG}
	${TRANSFER_WAIT}
	grep '^greeting$$' client.out
	grep '^command$$' server.out
	# XXX message Bound and Listening is redundant
	grep 'Bound on server.sock$$' server.err
	grep 'Listening on server.sock$$' server.err
	grep 'Connection received on server.sock$$' server.err
	# XXX message succeeded is missing
	! grep 'Connection to server.sock .* succeeded!' client.err

REGRESS_TARGETS +=	run-unix-namelookup
run-unix-namelookup:
	@echo '======== $@ ========'
	rm -f server.sock
	${SERVER_NC} -U -v -l server.sock ${SERVER_BG}
	${LISTEN_WAIT}
	${CLIENT_NC} -U -v server.sock ${CLIENT_BG}
	${TRANSFER_WAIT}
	grep '^greeting$$' client.out
	grep '^command$$' server.out
	# XXX message Bound and Listening is redundant
	grep 'Bound on server.sock$$' server.err
	grep 'Listening on server.sock$$' server.err
	grep 'Connection received on server.sock$$' server.err
	# XXX message succeeded is missing
	! grep 'Connection to server.sock .* succeeded!' client.err

REGRESS_TARGETS +=	run-unix-dgram
run-unix-dgram:
	@echo '======== $@ ========'
	rm -f {client,server}.sock
	${SERVER_NC} -U -u -n -v -l server.sock ${SERVER_BG}
	${BIND_WAIT}
	${CLIENT_NC} -U -u -n -v server.sock ${CLIENT_BG}
	${TRANSFER_WAIT}
	grep '^greeting$$' client.out
	grep '^command$$' server.out
	grep 'Bound on server.sock$$' server.err
	grep 'Connection received on server.sock$$' server.err
	# XXX message succeeded is missing
	! grep 'Connection to server.sock .* succeeded!' client.err

REGRESS_TARGETS +=	run-unix-dgram-namelookup
run-unix-dgram-namelookup:
	@echo '======== $@ ========'
	rm -f {client,server}.sock
	${SERVER_NC} -U -u -v -l server.sock ${SERVER_BG}
	${BIND_WAIT}
	${CLIENT_NC} -U -u -v server.sock ${CLIENT_BG}
	${TRANSFER_WAIT}
	grep '^greeting$$' client.out
	grep '^command$$' server.out
	grep 'Bound on server.sock$$' server.err
	grep 'Connection received on server.sock$$' server.err
	# XXX message succeeded is missing
	! grep 'Connection to server.sock .* succeeded!' client.err

REGRESS_TARGETS +=	run-unix-dgram-clientsock
run-unix-dgram-clientsock:
	@echo '======== $@ ========'
	rm -f {client,server}.sock
	${SERVER_NC} -U -u -n -v -l server.sock ${SERVER_BG}
	${BIND_WAIT}
	${CLIENT_NC} -U -u -n -v -s client.sock server.sock ${CLIENT_BG}
	${TRANSFER_WAIT}
	grep '^greeting$$' client.out
	grep '^command$$' server.out
	grep 'Bound on server.sock$$' server.err
	grep 'Connection received on server.sock$$' server.err
	# XXX message succeeded is missing
	! grep 'Connection to server.sock .* succeeded!' client.err

.PHONY: ${REGRESS_SETUP} ${REGRESS_CLEANUP} ${REGRESS_TARGETS}

### create certificates for TLS

CLEANFILES +=		{127.0.0.1,1}.{crt,key} \
			ca.{crt,key,srl} fake-ca.{crt,key} \
			{client,server}.{req,crt,key,hash}

127.0.0.1.crt:
	openssl req -batch -new \
	    -subj /L=OpenBSD/O=netcat-regress/OU=server/CN=${@:R}/ \
	    -nodes -newkey rsa -keyout ${@:R}.key -x509 -out $@

1.crt:
	openssl req -batch -new \
	    -subj /L=OpenBSD/O=netcat-regress/OU=server/CN=::1/ \
	    -nodes -newkey rsa -keyout 1.key -x509 -out $@

ca.crt fake-ca.crt:
	openssl req -batch -new \
	    -subj /L=OpenBSD/O=netcat-regress/OU=ca/CN=root/ \
	    -nodes -newkey rsa -keyout ${@:R}.key -x509 -out $@

client.req server.req:
	openssl req -batch -new \
	    -subj /L=OpenBSD/O=netcat-regress/OU=${@:R}/CN=localhost/ \
	    -nodes -newkey rsa -keyout ${@:R}.key -out $@

client.crt server.crt: ca.crt ${@:R}.req
	openssl x509 -CAcreateserial -CAkey ca.key -CA ca.crt \
	    -req -in ${@:R}.req -out $@

client.hash server.hash: ${@:R}.crt
	openssl x509 -in ${@:R}.crt -outform der | sha256 | sed s/^/SHA256:/ >$@

.include <bsd.regress.mk>
