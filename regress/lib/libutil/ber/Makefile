#	$OpenBSD: Makefile,v 1.3 2019/04/27 23:45:08 rob Exp $

REGRESS_TARGETS= run-regress run-regress-int-i

LDFLAGS= -lutil

CLEANFILES= ber_test ber_test.d
CLEANFILES+=ber_test_int_i ber_test_int_i.d

run-regress: ber_test
	./ber_test

run-regress-int-i: ber_test_int_i
	./ber_test_int_i

.include <bsd.regress.mk>
