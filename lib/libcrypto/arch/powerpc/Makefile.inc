# $OpenBSD: Makefile.inc,v 1.12 2024/03/29 07:24:09 jsing Exp $

# powerpc-specific libcrypto build rules

# slower than C code
#CFLAGS+= -DAES_ASM
#SSLASM+= aes aes-ppc aes-ppc
# bn
SSLASM+= bn ppc bn-ppc
SSLASM+= bn ppc-mont ppc-mont		# bn_mul_mont_int
#SSLASM+= bn ppc64-mont ppc64-mont	# bn_mul_mont_fpu64
CFLAGS+= -DOPENSSL_BN_ASM_MONT
# sha
CFLAGS+= -DSHA1_ASM
SSLASM+= sha sha1-ppc sha1-ppc
CFLAGS+= -DSHA256_ASM
SSLASM+= sha sha512-ppc sha256-ppc

.for dir src dst in ${SSLASM}
SRCS+=	${dst}.S
GENERATED+=${dst}.S
${dst}.S: ${LCRYPTO_SRC}/${dir}/asm/${src}.pl
	/usr/bin/perl \
		${LCRYPTO_SRC}/${dir}/asm/${src}.pl linux32 ${.TARGET} > ${.TARGET}
.endfor

#CFLAGS+= -DOPENSSL_CPUID_OBJ		# it's commented out in ppccap.c
SRCS+=	ppccap.c
