# $OpenBSD: Makefile,v 1.1 2020/09/15 01:45:16 bluhm Exp $

.if ! exists(/usr/local/bin/botan)
regress:
	# install botan2 from ports for interop tests
	@echo SKIPPED
.endif

LIBRARIES =		libressl
.if exists(/usr/local/bin/eopenssl)
LIBRARIES +=		openssl
.endif
.if exists(/usr/local/bin/eopenssl11)
LIBRARIES +=		openssl11
.endif

PROGS =		client
SRCS_client =	client.cpp
CXXFLAGS =	-I/usr/local/include/botan-2 -Wall
LDFLAGS =	-L/usr/local/lib
LDADD =		-lbotan-2
DPADD =		/usr/local/lib/libbotan-2.a

.for lib in ${LIBRARIES}

REGRESS_TARGETS += run-client-botan-server-${lib}

run-client-botan-server-${lib}: client server.crt
	@echo '\n======== $@ ========'
	LD_LIBRARY_PATH=/usr/local/lib/e${lib} \
	    ../${lib}/server >server-${lib}.out \
	    -c server.crt -k server.key \
	    127.0.0.1 0
	./client >client-botan.out \
	    -C ca.crt \
	    127.0.0.1 \
	    `sed -n 's/listen sock: 127.0.0.1 //p' server-${lib}.out`
	# check that the server child run successfully to the end
	grep -q '^success$$' server-${lib}.out || \
	    { sleep 1; grep -q '^success$$' server-${lib}.out; }
	# server must have read client hello
	grep -q '^<<< hello$$' server-${lib}.out
	# check that the client run successfully to the end
	grep -q '^success$$' client-botan.out
	# client must have read server greeting
	grep -q '^<<< greeting$$' client-botan.out
	# currently botan supports TLS 1.2, adapt later
	grep -q ' Protocol *: TLSv1.2$$' server-${lib}.out

.endfor

server.key ca.key:
	/usr/local/bin/botan keygen >$@.tmp
	mv $@.tmp $@

ca.crt: ${@:R}.key
	/usr/local/bin/botan gen_self_signed ${@:R}.key ${@:R} >$@.tmp \
	    --organization=tls-regress --ca
	mv $@.tmp $@

server.req: ${@:R}.key
	/usr/local/bin/botan gen_pkcs10 ${@:R}.key localhost >$@.tmp \
	    --organization=tls-regress --dns=127.0.0.1
	mv $@.tmp $@

server.crt: ca.crt ${@:R}.req
	/usr/local/bin/botan sign_cert ca.crt ca.key ${@:R}.req >$@.tmp
	mv $@.tmp $@

.include <bsd.regress.mk>
