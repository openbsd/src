LDADD +=	-Wl,-rpath,/usr/local/lib/eopenssl11 -L/usr/local/lib/eopenssl11
CFLAGS +=	-I/usr/local/include/eopenssl11/

# For mft.c we need ASN1_time_parse() and ASN1_time_tm_cmp() from LibreSSL
SRCS_test-mft =	a_time_tm.c o_time.c
CFLAGS +=	-I${.CURDIR}/../../../../lib/libcrypto/

.PATH:		${.CURDIR}/..
.PATH:		${.CURDIR}/../../../../lib/libcrypto
.PATH:		${.CURDIR}/../../../../lib/libcrypto/asn1

.include <bsd.regress.mk>
