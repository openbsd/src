#	$OpenBSD: Makefile,v 1.11 2022/11/18 06:43:39 anton Exp $

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

PROGS =			client-tcp server-tcp
SRCS_client-tcp =	client-tcp.c util.c
SRCS_server-tcp =	server-tcp.c util.c
WARNINGS =		yes

NC =			./netcat-regress

CLEANFILES =		${NC:T} {client,server}.{out,err,port,sock} ktrace.out

REGRESS_SETUP =		setup
setup:
	pkill ${NC:T} || true
	rm -f ${NC:T}
	# copying global netcat to local name allows to pkill it during cleanup
	cp /usr/bin/nc ${NC:T}
	chmod 755 ${NC:T}

REGRESS_CLEANUP =	cleanup
cleanup:
	-pkill ${NC:T} || true

REGRESS_TARGETS =

SERVER_NC = rm -f server.err; echo greeting | ${NC}
CLIENT_NC = rm -f client.err; echo command | ${NC}
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
	do [[ `date +%s` -lt $$timeout ]] || { echo timeout; exit 1; }; done

BIND_WAIT = \
	let timeout=`date +%s`+5; \
	until grep -q 'Bound on ' server.err; \
	do [[ `date +%s` -lt $$timeout ]] || { echo timeout; exit 1; }; done

BIND_CLIENT_WAIT = \
	let timeout=`date +%s`+5; \
	until grep -q 'Bound on ' client.err; \
	do [[ `date +%s` -lt $$timeout ]] || { echo timeout; exit 1; }; done

CONNECT_WAIT = \
	let timeout=`date +%s`+5; \
	until grep -q 'Connection to .* succeeded' client.err; \
	do [[ `date +%s` -lt $$timeout ]] || { echo timeout; exit 1; }; done

TLS_WAIT = \
	let timeout=`date +%s`+5; \
	until grep -q 'Cert Hash:' client.err; \
	do [[ `date +%s` -lt $$timeout ]] || { echo timeout; exit 1; }; done

TRANSFER_WAIT = \
	let timeout=`date +%s`+5; \
	until grep -q 'greeting' client.out && grep -q 'command' server.out; \
	do [[ `date +%s` -lt $$timeout ]] || { echo timeout; exit 1; }; done

TRANSFER_CLIENT_WAIT = \
	let timeout=`date +%s`+5; \
	until grep -q 'greeting' client.out; \
	do [[ `date +%s` -lt $$timeout ]] || { echo timeout; exit 1; }; done

TRANSFER_SERVER_WAIT = \
	let timeout=`date +%s`+5; \
	until grep -q 'command' server.out; \
	do [[ `date +%s` -lt $$timeout ]] || { echo timeout; exit 1; }; done

RUNNING_WAIT = \
	let timeout=`date +%s`+5; \
	while ps -xww -o comm,stat | grep -q '${NC:T} .*R'; \
	do [[ `date +%s` -lt $$timeout ]] || { echo timeout; exit 1; }; done

### TCP ####

REGRESS_TARGETS +=	run-tcp
run-tcp:
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
	! ${NC} -4 -v -l ::1 0 ${SERVER_LOG}
	grep 'non-recoverable failure in name resolution' server.err

REGRESS_TARGETS +=	run-tcp6-bad-localhost-server
run-tcp6-bad-localhost-server:
	! ${NC} -6 -v -l 127.0.0.0 0 ${SERVER_LOG}
	grep 'no address associated with name' server.err

REGRESS_TARGETS +=	run-tcp-bad-localhost-client
run-tcp-bad-localhost-client:
	${SERVER_NC} -n -v -l 127.0.0.1 0 ${SERVER_BG}
	${LISTEN_WAIT}
	${PORT_GET}
	! ${NC} -4 -v ::1 ${PORT} ${CLIENT_LOG}
	grep 'non-recoverable failure in name resolution' client.err

REGRESS_TARGETS +=	run-tcp6-bad-localhost-client
run-tcp6-bad-localhost-client:
	${SERVER_NC} -n -v -l 127.0.0.1 0 ${SERVER_BG}
	${LISTEN_WAIT}
	${PORT_GET}
	! ${NC} -6 -v 127.0.0.1 ${PORT} ${CLIENT_LOG}
	grep 'no address associated with name' client.err

REGRESS_TARGETS +=	run-tcp-sleep
run-tcp-sleep:
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
	# netcat waits for the other side to terminate, check it is sleeping
	${RUNNING_WAIT}
	ps -xww -o comm,stat,args | grep '^${NC:T} .*S.* -v -l 127'
	ps -xww -o comm,stat,args | grep '^${NC:T} .*S.* -v 127'

# TCP keep

REGRESS_TARGETS +=	run-tcp-keep
run-tcp-keep:
	${SERVER_NC} -k -n -v -l 127.0.0.1 0 ${SERVER_BG}
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
	# kill client and reconnect with a new one
	:> server.err
	pkill -l -f "^${NC} .* 127.0.0.1 ${PORT}$$"
	rm -f client.{out,err}
	:> server.out
	# server closes the listen socket and binds a new one with new port
	${LISTEN_WAIT}
	${PORT_GET}
	${CLIENT_NC} -n -v 127.0.0.1 ${PORT} ${CLIENT_BG}
	${CONNECT_WAIT}
	# server sends only one greeting, do not wait for a second one
	${TRANSFER_SERVER_WAIT}
	! grep 'greeting' client.out
	# truncation of log results in NUL bytes, do not match ^
	grep 'command$$' server.out
	grep 'Listening on 127.0.0.1 ' server.err
	grep 'Connection received on 127.0.0.1 ' server.err
	grep 'Connection to 127.0.0.1 .* succeeded!' client.err

### TLS ###

REGRESS_TARGETS +=	run-tls
run-tls: 127.0.0.1.crt
	${SERVER_NC} -c -C 127.0.0.1.crt -K 127.0.0.1.key -n -v -l 127.0.0.1 0 \
	    ${SERVER_BG}
	${LISTEN_WAIT}
	${PORT_GET}
	${CLIENT_NC} -c -R 127.0.0.1.crt -n -v 127.0.0.1 ${PORT} ${CLIENT_BG}
	${CONNECT_WAIT}
	${TLS_WAIT}
	${TRANSFER_WAIT}
	grep '^greeting$$' client.out
	grep '^command$$' server.out
	grep 'Listening on 127.0.0.1 ' server.err
	grep 'Connection received on 127.0.0.1 ' server.err
	# XXX success message should be issued after TLS handshake
	grep 'Connection to 127.0.0.1 .* succeeded!' client.err
	grep 'Subject: .*/OU=server/CN=127.0.0.1' client.err
	grep 'Issuer: .*/OU=server/CN=127.0.0.1' client.err

REGRESS_TARGETS +=	run-tls6
run-tls6: 1.crt
	${SERVER_NC} -c -C 1.crt -K 1.key -n -v -l ::1 0 ${SERVER_BG}
	${LISTEN_WAIT}
	${PORT_GET}
	${CLIENT_NC} -c -R 1.crt -n -v ::1 ${PORT} ${CLIENT_BG}
	${CONNECT_WAIT}
	${TLS_WAIT}
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
	${SERVER_NC} -c -C server.crt -K server.key -v -l localhost 0 \
	    ${SERVER_BG}
	${LISTEN_WAIT}
	${PORT_GET}
	${CLIENT_NC} -c -R ca.crt -v localhost ${PORT} ${CLIENT_BG}
	${CONNECT_WAIT}
	${TLS_WAIT}
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
	${SERVER_NC} -c -C server.crt -K server.key -v -l localhost 0 \
	    ${SERVER_BG}
	${LISTEN_WAIT}
	${PORT_GET}
	# the client uses the wrong root ca to verify the server cert
	! ${NC} -c -R fake-ca.crt -v localhost ${PORT} ${CLIENT_LOG}
	${CONNECT_WAIT}
	grep 'Listening on localhost ' server.err
	grep 'Connection received on localhost ' server.err
	grep 'certificate verification failed' client.err
	! grep 'greeting' client.out
	! grep 'command' server.out

REGRESS_TARGETS +=	run-tls-name
run-tls-name: server.crt ca.crt
	${SERVER_NC} -c -C server.crt -K server.key -n -v -l 127.0.0.1 0 \
	    ${SERVER_BG}
	${LISTEN_WAIT}
	${PORT_GET}
	${CLIENT_NC} -c -e localhost -R ca.crt -n -v 127.0.0.1 ${PORT} \
	    ${CLIENT_BG}
	${CONNECT_WAIT}
	${TLS_WAIT}
	${TRANSFER_WAIT}
	grep '^greeting$$' client.out
	grep '^command$$' server.out
	grep 'Listening on 127.0.0.1 ' server.err
	grep 'Connection received on 127.0.0.1 ' server.err
	grep 'Connection to 127.0.0.1 .* succeeded!' client.err
	grep 'Subject: .*/OU=server/CN=localhost' client.err
	grep 'Issuer: .*/OU=ca/CN=root' client.err

REGRESS_TARGETS +=	run-tls-bad-name
run-tls-bad-name: server.crt ca.crt
	${SERVER_NC} -c -C server.crt -K server.key -n -v -l 127.0.0.1 0 \
	    ${SERVER_BG}
	${LISTEN_WAIT}
	${PORT_GET}
	# the common name in server.crt is localhost, not 127.0.0.1
	! ${NC} -c -e 127.0.0.1 -R ca.crt -n -v 127.0.0.1 ${PORT} ${CLIENT_LOG}
	${CONNECT_WAIT}
	grep 'Listening on 127.0.0.1 ' server.err
	grep 'Connection received on 127.0.0.1 ' server.err
	grep 'Connection to 127.0.0.1 .* succeeded!' client.err
	grep "name \`127.0.0.1\' not present in server certificate" client.err
	! grep 'greeting' client.out
	! grep 'command' server.out

REGRESS_TARGETS +=	run-tls-hash
run-tls-hash: server.crt ca.crt server.hash
	${SERVER_NC} -c -C server.crt -K server.key -v -l localhost 0 \
	    ${SERVER_BG}
	${LISTEN_WAIT}
	${PORT_GET}
	# check that the server presents certificate with correct hash
	${CLIENT_NC} -c -H `cat server.hash` -R ca.crt -v localhost ${PORT} \
	    ${CLIENT_BG}
	${CONNECT_WAIT}
	${TLS_WAIT}
	${TRANSFER_WAIT}
	grep '^greeting$$' client.out
	grep '^command$$' server.out
	grep 'Listening on localhost ' server.err
	grep 'Connection received on localhost ' server.err
	grep 'Connection to localhost .* succeeded!' client.err
	grep 'Subject: .*/OU=server/CN=localhost' client.err
	grep 'Issuer: .*/OU=ca/CN=root' client.err
	grep 'Cert Hash: SHA256:' client.err

REGRESS_TARGETS +=	run-tls-bad-hash
run-tls-bad-hash: server.crt ca.crt ca.hash
	${SERVER_NC} -c -C server.crt -K server.key -v -l localhost 0 \
	    ${SERVER_BG}
	${LISTEN_WAIT}
	${PORT_GET}
	# server presents certificate with server.hash, ca.hash is wrong
	! ${NC} -c -H `cat ca.hash` -R ca.crt -v localhost ${PORT} \
	    ${CLIENT_LOG}
	${CONNECT_WAIT}
	${TLS_WAIT}
	grep 'Listening on localhost ' server.err
	grep 'Connection received on localhost ' server.err
	grep 'Connection to localhost .* succeeded!' client.err
	grep 'peer certificate is not SHA256:' client.err
	! grep 'greeting' client.out
	! grep 'command' server.out

# TLS client certificate

REGRESS_TARGETS +=	run-tls-client
run-tls-client: client.crt server.crt ca.crt
	# use client certificate and validate at server
	${SERVER_NC} -c -R ca.crt -C server.crt -K server.key -v -l \
	    localhost 0 ${SERVER_BG}
	${LISTEN_WAIT}
	${PORT_GET}
	${CLIENT_NC} -c -R ca.crt -C client.crt -K client.key -v \
	    localhost ${PORT} ${CLIENT_BG}
	${CONNECT_WAIT}
	${TLS_WAIT}
	${TRANSFER_WAIT}
	grep '^greeting$$' client.out
	grep '^command$$' server.out
	grep 'Listening on localhost ' server.err
	grep 'Connection received on localhost ' server.err
	grep 'Connection to localhost .* succeeded!' client.err
	grep 'Subject: .*/OU=server/CN=localhost' client.err
	grep 'Issuer: .*/OU=ca/CN=root' client.err
	grep 'Subject: .*/OU=client/CN=localhost' server.err
	grep 'Issuer: .*/OU=ca/CN=root' server.err

REGRESS_TARGETS +=	run-tls-bad-client
run-tls-bad-client: client.crt server.crt ca.crt
	# require client certificate at server
	${SERVER_NC} -c -T clientcert -R ca.crt -C server.crt -K server.key \
	    -v -l localhost 0 ${SERVER_BG}
	${LISTEN_WAIT}
	${PORT_GET}
	# client does not provide certificate
	${CLIENT_NC} -c -R ca.crt -v localhost ${PORT} ${CLIENT_BG}
	${CONNECT_WAIT}
	${TLS_WAIT}
	grep 'Listening on localhost ' server.err
	grep 'Connection received on localhost ' server.err
	grep 'Connection to localhost .* succeeded!' client.err
	grep 'Subject: .*/OU=server/CN=localhost' client.err
	grep 'Issuer: .*/OU=ca/CN=root' client.err
	grep 'No client certificate provided' server.err
	! grep 'greeting' client.out
	! grep 'command' server.out

REGRESS_TARGETS +=	run-tls-client-bad-ca
run-tls-client-bad-ca: client.crt server.crt ca.crt fake-ca.crt
	# the server uses the wrong root ca to verify the client cert
	${SERVER_NC} -c -R fake-ca.crt -C server.crt -K server.key -v -l \
	    localhost 0 ${SERVER_BG}
	${LISTEN_WAIT}
	${PORT_GET}
	! ${NC} -c -R ca.crt -C client.crt -K client.key -v \
	    localhost ${PORT} ${CLIENT_LOG}
	${CONNECT_WAIT}
	grep 'Listening on localhost ' server.err
	grep 'Connection received on localhost ' server.err
	grep 'Connection to localhost .* succeeded!' client.err
	# XXX no specific error message for bogus ca
	egrep \
	    'CRYPTO_internal:(block type is not 01|data too large for modulus)'\
	    server.err
	! grep 'greeting' client.out
	! grep 'command' server.out

REGRESS_TARGETS +=	run-tls-client-name
run-tls-client-name: client.crt server.crt ca.crt
	# check client certificate name at server
	${SERVER_NC} -c -e localhost -R ca.crt -C server.crt -K server.key \
	    -n -v -l 127.0.0.1 0 ${SERVER_BG}
	${LISTEN_WAIT}
	${PORT_GET}
	${CLIENT_NC} -4 -c -R ca.crt -C client.crt -K client.key -v \
	    localhost ${PORT} ${CLIENT_BG}
	${CONNECT_WAIT}
	${TLS_WAIT}
	${TRANSFER_WAIT}
	grep '^greeting$$' client.out
	grep '^command$$' server.out
	grep 'Listening on 127.0.0.1 ' server.err
	grep 'Connection received on 127.0.0.1 ' server.err
	grep 'Connection to localhost .* succeeded!' client.err
	grep 'Subject: .*/OU=server/CN=localhost' client.err
	grep 'Issuer: .*/OU=ca/CN=root' client.err
	grep 'Subject: .*/OU=client/CN=localhost' server.err
	grep 'Issuer: .*/OU=ca/CN=root' server.err

REGRESS_TARGETS +=	run-tls-client-bad-name
run-tls-client-bad-name: client.crt server.crt ca.crt
	# client certificate is for localhost, check with 127.0.0.1 should fail
	${SERVER_NC} -c -e 127.0.0.1 -R ca.crt -C server.crt -K server.key \
	    -n -v -l 127.0.0.1 0 ${SERVER_BG}
	${LISTEN_WAIT}
	${PORT_GET}
	# client does not see any problem, TLS handshake works, wait for exit
	${CLIENT_NC} -4 -c -R ca.crt -C client.crt -K client.key -v \
	    localhost ${PORT} ${CLIENT_BG}
	${CONNECT_WAIT}
	${TLS_WAIT}
	grep 'Listening on 127.0.0.1 ' server.err
	grep 'Connection received on 127.0.0.1 ' server.err
	grep 'Connection to localhost .* succeeded!' client.err
	grep 'Subject: .*/OU=server/CN=localhost' client.err
	grep 'Issuer: .*/OU=ca/CN=root' client.err
	grep 'Subject: .*/OU=client/CN=localhost' server.err
	grep 'Issuer: .*/OU=ca/CN=root' server.err
	grep 'name (127.0.0.1) not found in client cert' server.err
	! grep 'greeting' client.out
	! grep 'command' server.out

REGRESS_TARGETS +=	run-tls-client-hash
run-tls-client-hash: client.crt server.crt ca.crt client.hash
	# check client certificate hash at server
	${SERVER_NC} -c -H `cat client.hash` -R ca.crt \
	    -C server.crt -K server.key -v -l localhost 0 ${SERVER_BG}
	${LISTEN_WAIT}
	${PORT_GET}
	${CLIENT_NC} -c -R ca.crt -C client.crt -K client.key -v \
	    localhost ${PORT} ${CLIENT_BG}
	${CONNECT_WAIT}
	${TLS_WAIT}
	${TRANSFER_WAIT}
	grep '^greeting$$' client.out
	grep '^command$$' server.out
	grep 'Listening on localhost ' server.err
	grep 'Connection received on localhost ' server.err
	grep 'Connection to localhost .* succeeded!' client.err
	grep 'Subject: .*/OU=server/CN=localhost' client.err
	grep 'Issuer: .*/OU=ca/CN=root' client.err
	grep 'Subject: .*/OU=client/CN=localhost' server.err
	grep 'Issuer: .*/OU=ca/CN=root' server.err

REGRESS_TARGETS +=	run-tls-client-bad-hash
run-tls-client-bad-hash: client.crt server.crt ca.crt ca.hash
	# client presents certificate with client.hash, ca.hash is wrong
	${SERVER_NC} -c -H `cat ca.hash` -R ca.crt \
	    -C server.crt -K server.key -v -l localhost 0 ${SERVER_BG}
	${LISTEN_WAIT}
	${PORT_GET}
	# client does not see any problem, TLS handshake works, wait for exit
	${CLIENT_NC} -c -R ca.crt -C client.crt -K client.key -v \
	    localhost ${PORT} ${CLIENT_BG}
	${CONNECT_WAIT}
	${TLS_WAIT}
	grep 'Listening on localhost ' server.err
	grep 'Connection received on localhost ' server.err
	grep 'Connection to localhost .* succeeded!' client.err
	grep 'Subject: .*/OU=server/CN=localhost' client.err
	grep 'Issuer: .*/OU=ca/CN=root' client.err
	grep 'Subject: .*/OU=client/CN=localhost' server.err
	grep 'Issuer: .*/OU=ca/CN=root' server.err
	grep 'peer certificate is not SHA256:' server.err
	! grep 'greeting' client.out
	! grep 'command' server.out

REGRESS_TARGETS +=	run-tls-client-no-hash
run-tls-client-no-hash: client.crt server.crt ca.crt client.hash
	# check client certificate hash at server if available
	${SERVER_NC} -c -H `cat client.hash` -R ca.crt \
	    -C server.crt -K server.key -v -l localhost 0 ${SERVER_BG}
	${LISTEN_WAIT}
	${PORT_GET}
	# client provides no certificate
	${CLIENT_NC} -c -R ca.crt -v localhost ${PORT} ${CLIENT_BG}
	${CONNECT_WAIT}
	${TLS_WAIT}
	${TRANSFER_WAIT}
	# client certificate and hash is optional, transfer is successful
	grep '^greeting$$' client.out
	grep '^command$$' server.out
	grep 'Listening on localhost ' server.err
	grep 'Connection received on localhost ' server.err
	grep 'Connection to localhost .* succeeded!' client.err
	grep 'Subject: .*/OU=server/CN=localhost' client.err
	grep 'Issuer: .*/OU=ca/CN=root' client.err
	# non existing hash is not checked
	! grep 'Cert Hash: SHA256:' server.err

REGRESS_TARGETS +=	run-tls-sleep
run-tls-sleep: 127.0.0.1.crt
	${SERVER_NC} -c -C 127.0.0.1.crt -K 127.0.0.1.key -n -v -l 127.0.0.1 0 \
	    ${SERVER_BG}
	${LISTEN_WAIT}
	${PORT_GET}
	${CLIENT_NC} -c -R 127.0.0.1.crt -n -v 127.0.0.1 ${PORT} ${CLIENT_BG}
	${CONNECT_WAIT}
	${TLS_WAIT}
	${TRANSFER_WAIT}
	grep '^greeting$$' client.out
	grep '^command$$' server.out
	grep 'Listening on 127.0.0.1 ' server.err
	grep 'Connection received on 127.0.0.1 ' server.err
	# XXX success message should be issued after TLS handshake
	grep 'Connection to 127.0.0.1 .* succeeded!' client.err
	grep 'Subject: .*/OU=server/CN=127.0.0.1' client.err
	grep 'Issuer: .*/OU=server/CN=127.0.0.1' client.err
	# netcat waits for the other side to terminate, check it is sleeping
	${RUNNING_WAIT}
	ps -xww -o comm,stat,args | grep '^${NC:T} .*S.* -v -l 127'
	ps -xww -o comm,stat,args | grep '^${NC:T} .*S.* -v 127'

# TLS keep

REGRESS_TARGETS +=	run-tls-keep
run-tls-keep: 127.0.0.1.crt
	${SERVER_NC} -k -c -C 127.0.0.1.crt -K 127.0.0.1.key -n -v -l \
	    127.0.0.1 0 ${SERVER_BG}
	${LISTEN_WAIT}
	${PORT_GET}
	${CLIENT_NC} -c -R 127.0.0.1.crt -n -v 127.0.0.1 ${PORT} ${CLIENT_BG}
	${CONNECT_WAIT}
	${TLS_WAIT}
	${TRANSFER_WAIT}
	grep '^greeting$$' client.out
	grep '^command$$' server.out
	grep 'Listening on 127.0.0.1 ' server.err
	grep 'Connection received on 127.0.0.1 ' server.err
	grep 'Connection to 127.0.0.1 .* succeeded!' client.err
	grep 'Subject: .*/OU=server/CN=127.0.0.1' client.err
	grep 'Issuer: .*/OU=server/CN=127.0.0.1' client.err
	# kill client and reconnect with a new one
	:> server.err
	pkill -l -f "^${NC} .* 127.0.0.1 ${PORT}$$"
	rm -f client.{out,err}
	:> server.out
	# server closes the listen socket and binds a new one with new port
	${LISTEN_WAIT}
	${PORT_GET}
	${CLIENT_NC} -c -R 127.0.0.1.crt -n -v 127.0.0.1 ${PORT} ${CLIENT_BG}
	${CONNECT_WAIT}
	${TLS_WAIT}
	# server sends only one greeting, do not wait for a second one
	${TRANSFER_SERVER_WAIT}
	! grep 'greeting' client.out
	# truncation of log results in NUL bytes, do not match ^
	grep 'command$$' server.out
	grep 'Listening on 127.0.0.1 ' server.err
	grep 'Connection received on 127.0.0.1 ' server.err
	grep 'Connection to 127.0.0.1 .* succeeded!' client.err
	grep 'Subject: .*/OU=server/CN=127.0.0.1' client.err
	grep 'Issuer: .*/OU=server/CN=127.0.0.1' client.err

### UDP ####

REGRESS_TARGETS +=	run-udp
run-udp:
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

REGRESS_TARGETS +=	run-udp-probe
run-udp-probe:
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

# UDP keep

REGRESS_TARGETS +=	run-udp-keep
run-udp-keep:
	${SERVER_NC} -k -u -n -v -l 127.0.0.1 0 ${SERVER_BG}
	${BIND_WAIT}
	${PORT_GET}
	# the -v option causes udptest() to write additional X
	${CLIENT_NC} -u -n -v 127.0.0.1 ${PORT} ${CLIENT_BG}
	# server does not connect, nothing reaches the client
	${TRANSFER_SERVER_WAIT}
	! grep 'greeting' client.out
	grep '^XXXXcommand$$' server.out
	grep 'Bound on 127.0.0.1 ' server.err
	# client does not connect
	! grep 'Connection received on ' server.err
	grep 'Connection to 127.0.0.1 .* succeeded!' client.err
	# kill client and reconnect with a new one
	:> server.err
	pkill -l -f "^${NC} .* 127.0.0.1 ${PORT}$$"
	rm -f client.{out,err}
	:> server.out
	${CLIENT_NC} -u -n -v 127.0.0.1 ${PORT} ${CLIENT_BG}
	${TRANSFER_SERVER_WAIT}
	! grep 'greeting' client.out
	# truncation of log results in NUL bytes, do not match ^
	grep 'XXXXcommand$$' server.out
	# server keeps socket and does not bind again
	! grep 'Bound on ' server.err
	# client does not connect
	! grep 'Connection received on ' server.err
	grep 'Connection to 127.0.0.1 .* succeeded!' client.err

REGRESS_TARGETS +=	run-udp-sleep
run-udp-sleep:
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
	# netcat waits for the other side to terminate, check it is sleeping
	${RUNNING_WAIT}
	ps -xww -o comm,stat,args | grep '^${NC:T} .*S.* -v -l 127'
	ps -xww -o comm,stat,args | grep '^${NC:T} .*S.* -n 127'

### UNIX ####

REGRESS_TARGETS +=	run-unix
run-unix:
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

REGRESS_TARGETS +=	run-unix-probe
run-unix-probe:
	rm -f server.sock
	${SERVER_NC} -U -n -v -l server.sock ${SERVER_BG}
	${LISTEN_WAIT}
	# connect and close immediately, check if socket is listening
	${NC} -N -U -v server.sock </dev/null ${CLIENT_LOG}
	# XXX message Bound and Listening is redundant
	grep 'Bound on server.sock$$' server.err
	grep 'Listening on server.sock$$' server.err
	grep 'Connection received on server.sock$$' server.err
	# XXX message succeeded is missing
	! grep 'Connection to server.sock .* succeeded!' client.err
	# server accepts one connection, second connection should be refused
	! ${NC} -N -U -v server.sock </dev/null ${CLIENT_LOG}
	grep 'server.sock: Connection refused' client.err
	# connection to non existing socket file should fail
	rm server.sock
	! ${NC} -N -U -v server.sock </dev/null ${CLIENT_LOG}
	grep 'server.sock: No such file or directory' client.err

# UNIX keep

REGRESS_TARGETS +=	run-unix-keep
run-unix-keep:
	rm -f server.sock
	${SERVER_NC} -k -U -n -v -l server.sock ${SERVER_BG}
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
	# kill client and reconnect with a new one
	:> server.err
	pkill -l -f "^${NC} .* -v server.sock$$"
	rm -f client.{out,err}
	:> server.out
	${CLIENT_NC} -U -n -v server.sock ${CLIENT_BG}
	# server sends only one greeting, do not wait for a second one
	${TRANSFER_SERVER_WAIT}
	! grep 'greeting' client.out
	# truncation of log results in NUL bytes, do not match ^
	grep 'command$$' server.out
	grep 'Connection received on server.sock$$' server.err
	# XXX message succeeded is missing
	! grep 'Connection to server.sock .* succeeded!' client.err

# UNIX dgram

REGRESS_TARGETS +=	run-unix-dgram
run-unix-dgram:
	rm -f {client,server}.sock
	${SERVER_NC} -U -u -n -v -l server.sock ${SERVER_BG}
	${BIND_WAIT}
	${CLIENT_NC} -U -u -n -v server.sock ${CLIENT_BG}
	${TRANSFER_WAIT}
	${BIND_CLIENT_WAIT}
	grep '^greeting$$' client.out
	grep '^command$$' server.out
	grep 'Bound on server.sock$$' server.err
	grep 'Connection received on server.sock$$' server.err
	# XXX message succeeded is missing
	! grep 'Connection to server.sock .* succeeded!' client.err

REGRESS_TARGETS +=	run-unix-dgram-namelookup
run-unix-dgram-namelookup:
	rm -f {client,server}.sock
	${SERVER_NC} -U -u -v -l server.sock ${SERVER_BG}
	${BIND_WAIT}
	${CLIENT_NC} -U -u -v server.sock ${CLIENT_BG}
	${TRANSFER_WAIT}
	${BIND_CLIENT_WAIT}
	grep '^greeting$$' client.out
	grep '^command$$' server.out
	grep 'Bound on server.sock$$' server.err
	grep 'Connection received on server.sock$$' server.err
	# XXX message succeeded is missing
	! grep 'Connection to server.sock .* succeeded!' client.err

REGRESS_TARGETS +=	run-unix-dgram-clientsock
run-unix-dgram-clientsock:
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

# UNIX dgram keep

REGRESS_TARGETS +=	run-unix-dgram-keep
run-unix-dgram-keep:
	rm -f {client,server}.sock
	${SERVER_NC} -k -U -u -n -v -l server.sock ${SERVER_BG}
	${BIND_WAIT}
	${CLIENT_NC} -U -u -n -v server.sock ${CLIENT_BG}
	# server does not connect, nothing reaches the client
	${TRANSFER_SERVER_WAIT}
	${BIND_CLIENT_WAIT}
	! grep 'greeting' client.out
	grep '^command$$' server.out
	grep 'Bound on server.sock$$' server.err
	# client does not connect
	! grep 'Connection received on ' server.err
	# XXX message succeeded is missing
	! grep 'Connection to server.sock .* succeeded!' client.err
	# kill client and reconnect with a new one
	:> server.err
	pkill -l -f "^${NC} .* -v server.sock$$"
	rm -f client.{out,err}
	:> server.out
	${CLIENT_NC} -U -u -n -v server.sock ${CLIENT_BG}
	${TRANSFER_SERVER_WAIT}
	${BIND_CLIENT_WAIT}
	! grep 'greeting' client.out
	# truncation of log results in NUL bytes, do not match ^
	grep 'command$$' server.out
	# server keeps socket and does not bind again
	! grep 'Bound on ' server.err
	# client does not connect
	! grep 'Connection received on ' server.err
	# XXX message succeeded is missing
	! grep 'Connection to 127.0.0.1 .* succeeded!' client.err

### TCP with test peer

REGRESS_TARGETS +=	run-tcp-test
run-tcp-test: server-tcp client-tcp
	# test the test tools
	./server-tcp -s greeting -r command 127.0.0.1 0 >server.port
	./client-tcp -r greeting -s command 127.0.0.1 ${PORT} >client.port

REGRESS_TARGETS +=	run-tcp-test-shutdown
run-tcp-test-shutdown: server-tcp client-tcp
	# test the test tools
	./server-tcp -s greeting -N -r command -E 127.0.0.1 0 >server.port
	./client-tcp -r greeting -E -s command -N 127.0.0.1 ${PORT} >client.port

# TCP netcat server with test client

REGRESS_TARGETS +=	run-tcp-server
run-tcp-server: client-tcp
	${SERVER_NC} -n -v -l 127.0.0.1 0 ${SERVER_BG}
	${LISTEN_WAIT}
	${PORT_GET}
	# test client read from netcat, then send line and exit
	./client-tcp -r greeting -s command 127.0.0.1 ${PORT} >client.port
	${TRANSFER_SERVER_WAIT}
	grep '^command$$' server.out
	grep 'Listening on 127.0.0.1 ' server.err
	grep 'Connection received on 127.0.0.1 ' server.err

REGRESS_TARGETS +=	run-tcp-server-eof
run-tcp-server-eof: client-tcp
	${SERVER_NC} -n -v -l 127.0.0.1 0 ${SERVER_BG}
	${LISTEN_WAIT}
	${PORT_GET}
	# test client read from netcat, then send line, shutdown, wait for eof
	./client-tcp -r greeting -s command -N -E 127.0.0.1 ${PORT} >client.port
	${TRANSFER_SERVER_WAIT}
	grep '^command$$' server.out
	grep 'Listening on 127.0.0.1 ' server.err
	grep 'Connection received on 127.0.0.1 ' server.err

REGRESS_TARGETS +=	run-tcp-server-reverse-eof
run-tcp-server-reverse-eof: client-tcp
	${SERVER_NC} -n -v -l 127.0.0.1 0 ${SERVER_BG}
	${LISTEN_WAIT}
	${PORT_GET}
	# test client send to netcat, shutdown, then read line, wait for eof
	./client-tcp -s command -N -r greeting -E 127.0.0.1 ${PORT} >client.port
	${TRANSFER_SERVER_WAIT}
	grep '^command$$' server.out
	grep 'Listening on 127.0.0.1 ' server.err
	grep 'Connection received on 127.0.0.1 ' server.err

REGRESS_TARGETS +=	run-tcp-server-shutdown-eof
run-tcp-server-shutdown-eof: client-tcp
	# netcat calls shutdown on output after EOF on input
	${SERVER_NC} -N -n -v -l 127.0.0.1 0 ${SERVER_BG}
	${LISTEN_WAIT}
	${PORT_GET}
	# test client read from netcat, then send line, wait for eof, shutdown
	./client-tcp -r greeting -s command -E -N 127.0.0.1 ${PORT} >client.port
	${TRANSFER_SERVER_WAIT}
	grep '^command$$' server.out
	grep 'Listening on 127.0.0.1 ' server.err
	grep 'Connection received on 127.0.0.1 ' server.err

REGRESS_TARGETS +=	run-tcp-server-shutdown-reverse-eof
run-tcp-server-shutdown-reverse-eof: client-tcp
	# netcat calls shutdown on output after EOF on input
	${SERVER_NC} -N -n -v -l 127.0.0.1 0 ${SERVER_BG}
	${LISTEN_WAIT}
	${PORT_GET}
	# test client send to netcat, shutdown, then read line, wait for eof
	./client-tcp -s command -N -r greeting -E 127.0.0.1 ${PORT} >client.port
	${TRANSFER_SERVER_WAIT}
	grep '^command$$' server.out
	grep 'Listening on 127.0.0.1 ' server.err
	grep 'Connection received on 127.0.0.1 ' server.err

# TCP netcat client with test server

REGRESS_TARGETS +=	run-tcp-client
run-tcp-client: server-tcp
	# test server send to netcat, then read line and exit
	./server-tcp -s greeting -r command 127.0.0.1 0 >server.port
	${CLIENT_NC} -n -v 127.0.0.1 ${PORT} ${CLIENT_BG}
	${CONNECT_WAIT}
	${TRANSFER_CLIENT_WAIT}
	grep '^greeting$$' client.out
	grep 'Connection to 127.0.0.1 .* succeeded!' client.err

REGRESS_TARGETS +=	run-tcp-client-eof
run-tcp-client-eof: server-tcp
	# test server send to netcat, shutdown, then read line, wait for eof
	./server-tcp -s greeting -N -r command -E 127.0.0.1 0 >server.port
	${CLIENT_NC} -n -v 127.0.0.1 ${PORT} ${CLIENT_BG}
	${CONNECT_WAIT}
	${TRANSFER_CLIENT_WAIT}
	grep '^greeting$$' client.out
	grep 'Connection to 127.0.0.1 .* succeeded!' client.err

REGRESS_TARGETS +=	run-tcp-client-reverse-eof
run-tcp-client-reverse-eof: server-tcp
	# test server read from netcat, then read line, wait for eof, shutdown
	./server-tcp -r command -s greeting -E -N 127.0.0.1 0 >server.port
	${CLIENT_NC} -n -v 127.0.0.1 ${PORT} ${CLIENT_BG}
	${CONNECT_WAIT}
	${TRANSFER_CLIENT_WAIT}
	grep '^greeting$$' client.out
	grep 'Connection to 127.0.0.1 .* succeeded!' client.err

REGRESS_TARGETS +=	run-tcp-client-shutdown-eof
run-tcp-client-shutdown-eof: server-tcp
	# test server send to netcat, shutdown, then read line, wait for eof
	./server-tcp -s greeting -N -r command -E 127.0.0.1 0 >server.port
	# netcat calls shutdown on output after EOF on input
	${CLIENT_NC} -N -n -v 127.0.0.1 ${PORT} ${CLIENT_BG}
	${CONNECT_WAIT}
	${TRANSFER_CLIENT_WAIT}
	grep '^greeting$$' client.out
	grep 'Connection to 127.0.0.1 .* succeeded!' client.err

REGRESS_TARGETS +=	run-tcp-client-shutdown-reverse-eof
run-tcp-client-shutdown-reverse-eof: server-tcp
	# test server read from netcat, wait for eof, then read line, shutdown
	./server-tcp -r command -E -s greeting -N 127.0.0.1 0 >server.port
	# netcat calls shutdown on output after EOF on input
	${CLIENT_NC} -N -n -v 127.0.0.1 ${PORT} ${CLIENT_BG}
	${CONNECT_WAIT}
	${TRANSFER_CLIENT_WAIT}
	grep '^greeting$$' client.out
	grep 'Connection to 127.0.0.1 .* succeeded!' client.err


.PHONY: ${REGRESS_SETUP} ${REGRESS_CLEANUP} ${REGRESS_TARGETS}

### create certificates for TLS

CLEANFILES +=		{127.0.0.1,1}.{crt,key} \
			ca.{crt,key,srl,hash} fake-ca.{crt,key,hash} \
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

client.hash server.hash ca.hash: ${@:R}.crt
	openssl x509 -in ${@:R}.crt -outform der | sha256 | sed s/^/SHA256:/ >$@

.include <bsd.regress.mk>
