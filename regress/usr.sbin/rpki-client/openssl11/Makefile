LDADD +=	-Wl,-rpath,/usr/local/lib/eopenssl11 -L/usr/local/lib/eopenssl11
CFLAGS +=	-I/usr/local/include/eopenssl11/

# For mft.c we need ASN1_time_parse() and ASN1_time_tm_cmp() from LibreSSL

# Provide a missing prototype
a_time_tm_gen.c: a_time_tm.c
	echo '#include <openssl/asn1.h>\n' > $@.tmp
	echo 'void ASN1error(int);' >> $@.tmp
	cat $> >> $@.tmp
	mv -f $@.tmp $@

CLEANFILES += a_time_tm_gen.c a_time_tm_gen.c.tmp

SRCS_test-mft =	a_time_tm_gen.c o_time.c
SRCS_test-roa =	a_time_tm_gen.c o_time.c
CFLAGS +=	-I${.CURDIR}/../../../../lib/libcrypto/

.PATH:		${.CURDIR}/..
.PATH:		${.CURDIR}/../../../../lib/libcrypto
.PATH:		${.CURDIR}/../../../../lib/libcrypto/asn1

.include <bsd.regress.mk>
