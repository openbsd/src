# $OpenBSD: Makefile,v 1.2 2026/01/25 14:57:43 tb Exp $

EGCC = /usr/local/bin/egcc

.if !exists(${EGCC})
regress:
	@echo package gcc is required for this regress
	@echo SKIPPED
.else

.if ${MACHINE_ARCH} == "aarch64"
ASSEMBLY += sha/sha1_aarch64_ce.S
ASSEMBLY += sha/sha256_aarch64_ce.S
ASSEMBLY += sha/sha512_aarch64_ce.S
.elif ${MACHINE_ARCH} == "amd64"
ASSEMBLY += md5/md5_amd64_generic.S
ASSEMBLY += sha/sha1_amd64_generic.S
ASSEMBLY += sha/sha1_amd64_shani.S
ASSEMBLY += sha/sha256_amd64_generic.S
ASSEMBLY += sha/sha256_amd64_shani.S
ASSEMBLY += sha/sha512_amd64_generic.S
.endif

.for assembly in ${ASSEMBLY}
regress-${assembly}:
	@echo "Checking ${assembly} compiles with gcc"
	@${EGCC} -o /dev/null -c ${.CURDIR}/../../../../lib/libcrypto/${assembly}

REGRESS_TARGETS += regress-${assembly}

.endfor

.endif

.include <bsd.regress.mk>
