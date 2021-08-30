# $OpenBSD: Makefile,v 1.10 2021/08/30 17:50:05 tb Exp $

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

REGRESS_TARGETS +=	test-verify-unusual-wildcard-cert
REGRESS_TARGETS +=	test-openssl-verify-common-wildcard-cert
REGRESS_TARGETS +=	test-alternative-chain

REGRESS_CLEANUP =	cleanup-ssl
REGRESS_SETUP_ONCE =	create-libressl-test-certs

create-libressl-test-certs: create-libressl-test-certs.pl
	${PERL} ${.CURDIR}/$@.pl

cleanup-ssl:
	rm *.pem *.key

test-verify-unusual-wildcard-cert:
	# openssl verify, unusual wildcard cert
	${OPENSSL} verify -CAfile caR.pem server-unusual-wildcard.pem \
	    | grep "server-unusual-wildcard.pem: OK"

test-openssl-verify-common-wildcard-cert:
	# openssl verify, common wildcard cert
	${OPENSSL} verify -CAfile caR.pem server-common-wildcard.pem \
	    | grep "server-common-wildcard.pem: OK"

test-alternative-chain:
	# alternative chain not found
	${OPENSSL} verify -verbose -trusted caR.pem -untrusted chainSX.pem \
	   server-subca.pem | grep "server-subca.pem: OK"

.include <bsd.regress.mk>
