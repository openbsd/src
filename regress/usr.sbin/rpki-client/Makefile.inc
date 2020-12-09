# $OpenBSD: Makefile.inc,v 1.4 2020/12/09 11:30:44 claudio Exp $

.PATH:		${.CURDIR}/../../../../usr.sbin/rpki-client

PROGS += test-ip
PROGS += test-cert
PROGS += test-mft
PROGS += test-roa
PROGS += test-gbr
PROGS += test-tal

.for p in ${PROGS}
REGRESS_TARGETS += run-regress-$p
.endfor

CFLAGS+=	-I${.CURDIR}/.. -I${.CURDIR}/../../../../usr.sbin/rpki-client
LDADD+=		-lcrypto
DPADD+=		${LIBCRYPTO}

SRCS_test-ip=	test-ip.c ip.c io.c
run-regress-test-ip: test-ip
	./test-ip

TALARGS += ta/AfriNIC.cer			tal/afrinic.tal
TALARGS += ta/apnic-rpki-root-iana-origin.cer	tal/apnic.tal
TALARGS += ta/ripe-ncc-ta.cer			tal/ripe.tal

SRCS_test-cert=	test-cert.c cert.c x509.c ip.c as.c rsync.c io.c log.c tal.c
run-regress-test-cert: test-cert
	./test-cert -v ${.CURDIR}/../cer/*.cer
	./test-cert -vt ${TALARGS:S,,${.CURDIR}/../&,}

# Provide missing prototypes for OpenSSL
mft_gen.c: mft.c
	echo '#include <openssl/asn1.h>\n' > $@.tmp
	echo 'int ASN1_time_parse(const char *, size_t, struct tm *, int);' \
	      	>> $@.tmp
	echo 'int ASN1_time_tm_cmp(struct tm *, struct tm *);' >> $@.tmp
	cat $> >> $@.tmp
	mv -f $@.tmp $@

CLEANFILES += mft_gen.c mft_gen.c.tmp

SRCS_test-mft+=	test-mft.c mft_gen.c cms.c x509.c io.c log.c
run-regress-test-mft: test-mft
	./test-mft -v ${.CURDIR}/../mft/*.mft

SRCS_test-roa=	test-roa.c roa.c cms.c x509.c ip.c as.c io.c log.c
run-regress-test-roa: test-roa
	./test-roa -v ${.CURDIR}/../roa/*.roa

SRCS_test-gbr=	test-gbr.c gbr.c cms.c x509.c io.c log.c
run-regress-test-gbr: test-gbr
	./test-gbr -v ${.CURDIR}/../gbr/*.gbr

SRCS_test-tal=	test-tal.c tal.c rsync.c io.c log.c
run-regress-test-tal: test-tal
	./test-tal -v ${.CURDIR}/../tal/*.tal
