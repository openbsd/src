# $OpenBSD: Makefile,v 1.4 2019/08/12 18:03:17 jsing Exp $

.PATH:		${.CURDIR}/../../../usr.sbin/rpki-client

.if ! exists(/usr/local/bin/eopenssl)
regress:
	# install openssl-1.0.2 from ports for interop tests
	@echo SKIPPED
.else
PROGS += test-ip
PROGS += test-cert
PROGS += test-mft
PROGS += test-roa
PROGS += test-tal
.endif

.for p in ${PROGS}
REGRESS_TARGETS += run-regress-$p
.endfor

CFLAGS+=	-I${.CURDIR} -I${.CURDIR}/../../../usr.sbin/rpki-client \
		-I/usr/local/include/eopenssl
LDADD+=		/usr/local/lib/eopenssl/libcrypto.a	

SRCS_test-ip=	test-ip.c ip.c io.c
run-regress-test-ip: test-ip
	./test-ip

SRCS_test-cert=	test-cert.c cert.c x509.c ip.c as.c rsync.c io.c log.c
run-regress-test-cert: test-cert
	./test-cert -v ${.CURDIR}/cer/*.cer
	./test-cert -vt ${.CURDIR}/ta/*.cer

SRCS_test-mft=	test-mft.c mft.c cms.c x509.c io.c log.c
run-regress-test-mft: test-mft
	./test-mft -v ${.CURDIR}/mft/*.mft

SRCS_test-roa=	test-roa.c roa.c cms.c x509.c ip.c as.c io.c log.c
run-regress-test-roa: test-roa
	./test-roa -v ${.CURDIR}/roa/*.roa

SRCS_test-tal=	test-tal.c tal.c rsync.c io.c log.c
run-regress-test-tal: test-tal
	./test-tal -v ${.CURDIR}/tal/*.tal

.include <bsd.regress.mk>
