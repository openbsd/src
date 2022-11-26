# $OpenBSD: Makefile,v 1.16 2022/11/26 12:09:34 job Exp $

LDADD +=	-Wl,-rpath,/usr/local/lib/eopenssl11 -L/usr/local/lib/eopenssl11
CFLAGS +=	-I${.CURDIR}/ -I/usr/local/include/eopenssl11/
CFLAGS +=	-DLIBRESSL_INTERNAL

# For mft.c we need ASN1_time_parse() and ASN1_time_tm_cmp() from LibreSSL

# Provide a missing prototype
a_time_tm_gen.c: a_time_tm.c
	echo '#include <openssl/asn1.h>\n' > $@.tmp
	echo '#include "bytestring.h"\n' >> $@.tmp
	echo '#define ASN1error(err) ASN1err(0, (err));' >> $@.tmp
	cat $> >> $@.tmp
	mv -f $@.tmp $@

CLEANFILES += a_time_tm_gen.c a_time_tm_gen.c.tmp

LIBCRYPTO_COMPAT +=	a_time_tm_gen.c a_time_posix.c
LIBCRYPTO_COMPAT +=	bs_ber.c bs_cbb.c bs_cbs.c

SRCS_test-ip =		${LIBCRYPTO_COMPAT}
SRCS_test-mft =		${LIBCRYPTO_COMPAT}
SRCS_test-roa =		${LIBCRYPTO_COMPAT}
SRCS_test-cert =	${LIBCRYPTO_COMPAT}
SRCS_test-gbr =		${LIBCRYPTO_COMPAT}
SRCS_test-geofeed=	${LIBCRYPTO_COMPAT}
SRCS_test-tal =		${LIBCRYPTO_COMPAT}
SRCS_test-bgpsec =	${LIBCRYPTO_COMPAT}
SRCS_test-rrdp =	${LIBCRYPTO_COMPAT}
SRCS_test-rsc =		${LIBCRYPTO_COMPAT}
SRCS_test-aspa =	${LIBCRYPTO_COMPAT}
SRCS_test-tak =		${LIBCRYPTO_COMPAT}

CFLAGS +=	-I${.CURDIR}/../../../../lib/libcrypto/
CFLAGS +=	-I${.CURDIR}/../../../../lib/libcrypto/asn1
CFLAGS +=	-I${.CURDIR}/../../../../lib/libcrypto/bytestring

.PATH:		${.CURDIR}/..
.PATH:		${.CURDIR}/../../../../lib/libcrypto
.PATH:		${.CURDIR}/../../../../lib/libcrypto/asn1
.PATH:		${.CURDIR}/../../../../lib/libcrypto/bytestring

.include <bsd.regress.mk>
