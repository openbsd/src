# $OpenBSD: Makefile,v 1.2 2023/12/17 08:32:26 tb Exp $

C2SP_TESTVECTORS = /usr/local/share/c2sp-testvectors/

.if !exists(${C2SP_TESTVECTORS}) || !exists(/usr/local/bin/go)
regress:
	@echo required packages: security/c2sp-testvectors lang/go
	@echo optional packages: security/openssl/*
	@echo SKIPPED
.else

PROGS += cctv
SRCS_cctv =

cctv: cctv.go
	go build -o $@ ${.CURDIR}/cctv.go

OSSL_LIB =	/usr/local/lib/eopenssl
OSSL_INC =	/usr/local/include/eopenssl

. for V in 11 31 32
.  if exists(/usr/local/bin/eopenssl$V)
PROGS +=	cctv-openssl$V
SRCS_cctv-openssl$V =

CGO_CFLAGS_$V += 	-I${OSSL_INC}$V
CGO_LDFLAGS_$V += 	-Wl,-rpath,${OSSL_LIB}$V
CGO_LDFLAGS_$V += 	-L${OSSL_LIB}$V

cctv-openssl$V: cctv.go
	env CGO_CFLAGS="${CGO_CFLAGS_$V}" CGO_LDFLAGS="${CGO_LDFLAGS_$V}" \
	    go build -o $@ ${.CURDIR}/cctv.go
.  endif
. endfor

.endif

.include <bsd.regress.mk>
