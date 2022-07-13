# $OpenBSD: Makefile,v 1.6 2022/07/13 06:40:24 tb Exp $

WYCHEPROOF_TESTVECTORS = /usr/local/share/wycheproof/testvectors/

.if !exists(${WYCHEPROOF_TESTVECTORS})
regress:
	@echo package wycheproof-testvectors is required for this regress
	@echo package go should be installed if available
	@echo SKIPPED
.else

REGRESS_TARGETS += regress-wycheproof-primes

. if exists(/usr/local/bin/go)

REGRESS_TARGETS += regress-wycheproof

CLEANFILES +=	wycheproof

audit: wycheproof
	./wycheproof -v

wycheproof: wycheproof.go
	go build -o wycheproof ${.CURDIR}/wycheproof.go

regress-wycheproof: wycheproof
	./wycheproof

.PHONY: audit

. endif

PROGS += wycheproof-primes

LDADD +=	-lcrypto
DPADD +=	${LIBCRYPTO}
CFLAGS +=	-I${.CURDIR} -I${.OBJDIR}

primality_testcases.h: wycheproof-json.pl ${WYCHEPROOF_TESTVECTORS}/primality_test.json
	perl ${.CURDIR}/wycheproof-json.pl > $@.tmp
	mv -f $@.tmp $@

wycheproof-primes: wycheproof-primes.c primality_testcases.h

regress-wycheproof-primes: primality_testcases.h wycheproof-primes
	./wycheproof-primes

CLEANFILES +=	primality_testcases.h

.endif

.include <bsd.regress.mk>
