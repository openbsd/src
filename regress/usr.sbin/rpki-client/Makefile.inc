# $OpenBSD: Makefile.inc,v 1.27 2022/11/26 12:09:34 job Exp $

.PATH:		${.CURDIR}/../../../../usr.sbin/rpki-client

PROGS += test-ip
PROGS += test-cert
PROGS += test-gbr
PROGS += test-geofeed
PROGS += test-mft
PROGS += test-roa
PROGS += test-rsc
PROGS += test-tal
PROGS += test-rrdp
PROGS += test-aspa
PROGS += test-tak

.for p in ${PROGS}
REGRESS_TARGETS += run-regress-$p
.endfor

CFLAGS+=	-I${.CURDIR}/.. -I${.CURDIR}/../../../../usr.sbin/rpki-client
LDADD+=		-lcrypto -lutil
DPADD+=		${LIBCRYPTO} ${LIBUTIL}

SRCS_test-ip +=		test-ip.c ip.c io.c encoding.c print.c x509.c \
			log.c validate.c as.c cert.c cms.c mft.c
run-regress-test-ip: test-ip
	./test-ip

TALARGS += ta/AfriNIC.cer			tal/afrinic.tal
TALARGS += ta/apnic-rpki-root-iana-origin.cer	tal/apnic.tal
TALARGS += ta/ripe-ncc-ta.cer			tal/ripe.tal

SRCS_test-cert+=	test-cert.c cert.c cms.c x509.c ip.c as.c io.c \
			log.c tal.c validate.c encoding.c print.c mft.c
run-regress-test-cert: test-cert
	./test-cert -v ${.CURDIR}/../cer/*.cer
	./test-cert -vt ${TALARGS:S,,${.CURDIR}/../&,}

SRCS_test-mft+=	test-mft.c mft.c cms.c x509.c ip.c io.c log.c validate.c \
		encoding.c print.c dummy.c
run-regress-test-mft: test-mft
	./test-mft -v ${.CURDIR}/../mft/*.mft

SRCS_test-roa+=	test-roa.c roa.c cms.c x509.c ip.c as.c io.c log.c \
		encoding.c print.c validate.c cert.c mft.c
run-regress-test-roa: test-roa
	./test-roa -v ${.CURDIR}/../roa/*.roa

SRCS_test-rsc+=	test-rsc.c rsc.c cms.c x509.c ip.c as.c io.c log.c \
		encoding.c print.c validate.c cert.c mft.c
run-regress-test-rsc: test-rsc
	./test-rsc -v ${.CURDIR}/../rsc/*.sig

SRCS_test-gbr+=	test-gbr.c gbr.c cms.c x509.c ip.c io.c log.c \
		encoding.c print.c validate.c as.c cert.c mft.c
run-regress-test-gbr: test-gbr
	./test-gbr -v ${.CURDIR}/../gbr/*.gbr

SRCS_test-geofeed+=	test-geofeed.c geofeed.c cms.c x509.c ip.c io.c log.c \
		encoding.c print.c validate.c as.c cert.c mft.c
run-regress-test-geofeed: test-geofeed
	./test-geofeed -v ${.CURDIR}/../geofeed/*.csv

SRCS_test-tal+=	test-tal.c tal.c ip.c io.c log.c validate.c \
		encoding.c print.c dummy.c x509.c
run-regress-test-tal: test-tal
	./test-tal -v ${.CURDIR}/../tal/*.tal

SRCS_test-aspa+=	test-aspa.c aspa.c cms.c x509.c ip.c as.c io.c log.c \
		encoding.c print.c validate.c cert.c mft.c
run-regress-test-aspa: test-aspa
	./test-aspa -v ${.CURDIR}/../aspa/*.asa

SRCS_test-tak+=	test-tak.c tak.c cms.c x509.c ip.c as.c io.c log.c \
		encoding.c print.c validate.c cert.c mft.c
run-regress-test-tak: test-tak
	./test-tak -v ${.CURDIR}/../tak/*.tak

SRCS_test-rrdp+=	test-rrdp.c rrdp_delta.c rrdp_notification.c \
			rrdp_snapshot.c rrdp_util.c \
			log.c encoding.c ip.c validate.c dummy.c x509.c
LDADD_test-rrdp+=	-lexpat ${LDADD}
DPADD_test-rrdp+=	${LIBEXPAT} ${DPADD}
run-regress-test-rrdp: test-rrdp
	./test-rrdp \
	-n < ${.CURDIR}/../rrdp/notification.xml 2>&1 | tee rrdp-r1.out
	cmp ${.CURDIR}/../rrdp/rrdp-r1.out rrdp-r1.out

	./test-rrdp -S 8fe05c2e-047d-49e7-8398-cd4250a572b1 -N 50500 \
	-n < ${.CURDIR}/../rrdp/notification.xml 2>&1 | tee rrdp-r2.out
	cmp ${.CURDIR}/../rrdp/rrdp-r2.out rrdp-r2.out
	
	./test-rrdp -S 9b3f7e31-4979-ef8c-d818-73e4dadc3e6b -N 13755 \
	-H 7bbab52b9ea9caab08da761d20f74711e139a334777fa4141f0503885d6592d5 \
	-d < ${.CURDIR}/../rrdp/delta.xml 2>&1 | tee rrdp-r3.out
	cmp ${.CURDIR}/../rrdp/rrdp-r3.out rrdp-r3.out

	./test-rrdp -S 7e7d2563-5bbb-40b0-8723-6a2e90c85d9e -N 28483 \
	-H 2a051bfd199150fe6bcdc777d09e70fe1acdf239fbf98ba378a793685e5adb21 \
	-s < ${.CURDIR}/../rrdp/snapshot.xml 2>&1 | tee rrdp-r4.out
	cmp ${.CURDIR}/../rrdp/rrdp-r4.out rrdp-r4.out

	./test-rrdp \
	-n < ${.CURDIR}/../rrdp/xxe.xml 2>&1 | tee rrdp-r5.out
	cmp ${.CURDIR}/../rrdp/rrdp-r5.out rrdp-r5.out

	./test-rrdp \
	-n < ${.CURDIR}/../rrdp/billion_lol.xml 2>&1 | tee rrdp-r6.out
	cmp ${.CURDIR}/../rrdp/rrdp-r6.out rrdp-r6.out

	./test-rrdp \
	-n < ${.CURDIR}/../rrdp/notification_xmlns.xml 2>&1 | tee rrdp-r7.out
	cmp ${.CURDIR}/../rrdp/rrdp-r7.out rrdp-r7.out
	@echo OK
