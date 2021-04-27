# $OpenBSD: Makefile,v 1.4 2021/04/27 05:14:25 tb Exp $

# Copyright (c) 2021 Jan Klemkow <j.klemkow@wemelug.de>
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

# This regression test is based on manual test descriptions from:
# https://github.com/noxxi/libressl-tests

# The following port must be installed for the regression tests:
# p5-IO-Socket-SSL	perl interface to SSL sockets

PERL =		perl
OPENSSL ?=	openssl

.if !(make(clean) || make(cleandir) || make(obj))
. if !exists(/usr/local/libdata/perl5/site_perl/IO/Socket/SSL.pm)
regress:
	@echo "missing package p5-IO-Socket-SSL"
	@echo SKIPPED
. endif
.endif

REGRESS_TARGETS +=	test-inlabel-wildcard-cert-no-CA-client
REGRESS_TARGETS +=	test-inlabel-wildcard-cert-CA-client
REGRESS_TARGETS +=	test-common-wildcard-cert-no-CA-client
REGRESS_TARGETS +=	test-common-wildcard-cert-CA-client
REGRESS_TARGETS +=	test-verify-unusual-wildcard-cert
REGRESS_TARGETS +=	test-openssl-verify-common-wildcard-cert
REGRESS_TARGETS +=	test-chain-certificates-s_server
REGRESS_TARGETS +=	test-alternative-chain
REGRESS_CLEANUP =	cleanup-ssl
REGRESS_SETUP_ONCE =	create-libressl-test-certs

REGRESS_EXPECTED_FAILURES +=	test-inlabel-wildcard-cert-no-CA-client
REGRESS_EXPECTED_FAILURES +=	test-unusual-wildcard-cert-no-CA-client
REGRESS_EXPECTED_FAILURES +=	test-common-wildcard-cert-no-CA-client
REGRESS_EXPECTED_FAILURES +=	test-common-wildcard-cert-CA-client

create-libressl-test-certs: create-libressl-test-certs.pl
	${PERL} ${.CURDIR}/$@.pl

cleanup-ssl:
	rm *.pem *.key

test-inlabel-wildcard-cert-no-CA-client:
	# unusual wildcard cert, no CA given to client
	# start server
	${OPENSSL} s_server -cert server-unusual-wildcard.pem \
	    -key server-unusual-wildcard.pem & \
	    timeout=$$(($$(date +%s) + 5)); \
	    while fstat -p $$! | ! grep -q 'tcp .* \*:4433$$'; \
		do test $$(date +%s) -lt $$timeout || exit 1; done
	# start client
	echo "Q" | ${OPENSSL} s_client -verify_return_error \
	    | grep "Verify return code: 21"

test-inlabel-wildcard-cert-CA-client:
	# unusual wildcard cert, CA given to client
	# start server
	${OPENSSL} s_server -cert server-unusual-wildcard.pem \
	    -key server-unusual-wildcard.pem & \
	    timeout=$$(($$(date +%s) + 5)); \
	    while fstat -p $$! | ! grep -q 'tcp .* \*:4433$$'; \
		do test $$(date +%s) -lt $$timeout || exit 1; done
	# start client
	echo "Q" | ${OPENSSL} s_client -CAfile caR.pem \
	    | grep "Verify return code: 0"

test-common-wildcard-cert-no-CA-client:
	# common wildcard cert, no CA given to client
	# start server
	${OPENSSL} s_server -cert server-common-wildcard.pem \
	    -key server-common-wildcard.pem & \
	    timeout=$$(($$(date +%s) + 5)); \
	    while fstat -p $$! | ! grep -q 'tcp .* \*:4433$$'; \
		do test $$(date +%s) -lt $$timeout || exit 1; done
	# start client
	echo "Q" | ${OPENSSL} s_client \
	    | grep "Verify return code: 21"

test-common-wildcard-cert-CA-client:
	# common wildcard cert, CA given to client
	# start server
	${OPENSSL} s_server -cert server-unusual-wildcard.pem \
	    -key server-unusual-wildcard.pem & \
	    timeout=$$(($$(date +%s) + 5)); \
	    while fstat -p $$! | ! grep -q 'tcp .* \*:4433$$'; \
		do test $$(date +%s) -lt $$timeout || exit 1; done
	# start client
	echo "Q" | ${OPENSSL} s_client -CAfile caR.pem \
	    | grep "Verify return code: 21"

test-verify-unusual-wildcard-cert:
	# openssl verify, unusual wildcard cert
	${OPENSSL} verify -CAfile caR.pem server-unusual-wildcard.pem \
	    | grep "server-unusual-wildcard.pem: OK"

test-openssl-verify-common-wildcard-cert:
	# openssl verify, common wildcard cert
	${OPENSSL} verify -CAfile caR.pem server-common-wildcard.pem \
	    | grep "server-common-wildcard.pem: OK"

test-chain-certificates-s_server:
	# Not all chain certificates are sent in s_server
	# start server
	# ${OPENSSL} s_server -cert server-subca.pem        -CAfile subcaR.pem
	${OPENSSL} s_server -cert server-subca-chainS.pem -CAfile subcaR.pem & \
	    timeout=$$(($$(date +%s) + 5)); \
	    while fstat -p $$! | ! grep -q 'tcp .* \*:4433$$'; \
		do test $$(date +%s) -lt $$timeout || exit 1; done
	# start client
	 ${OPENSSL} s_client -CAfile caR.pem | grep "Verify return code: 0"

test-alternative-chain:
	# alternative chain not found
	${OPENSSL} verify -verbose -trusted caR.pem -untrusted chainSX.pem \
	   server-subca.pem | grep "server-subca.pem: OK"

.include <bsd.regress.mk>
