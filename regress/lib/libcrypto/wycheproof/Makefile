# $OpenBSD: Makefile,v 1.9 2023/07/08 19:41:07 tb Exp $

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

wycheproof: wycheproof.go
	go build -o wycheproof ${.CURDIR}/wycheproof.go

regress-wycheproof: wycheproof
	./wycheproof

. endif

PROGS += wycheproof-primes

LDADD +=	-lcrypto
DPADD +=	${LIBCRYPTO}
CFLAGS +=	-I${.CURDIR} -I${.OBJDIR}

primality_testcases.h: wycheproof-json.pl ${WYCHEPROOF_TESTVECTORS}/primality_test.json
	perl ${.CURDIR}/wycheproof-json.pl > $@.tmp && mv $@.tmp $@

wycheproof-primes.o: primality_testcases.h

regress-wycheproof-primes: wycheproof-primes
	./wycheproof-primes

CLEANFILES +=	primality_testcases.h

.endif

.include <bsd.regress.mk>
