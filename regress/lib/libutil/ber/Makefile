#	$OpenBSD: Makefile,v 1.2 2019/04/27 15:56:02 rob Exp $

REGRESS_TARGETS= run-regress run-regress-int-i

CFLAGS= -lutil

run-regress: ber_test
	./ber_test

run-regress-int-i: ber_test_int_i
	./ber_test_int_i

.include <bsd.regress.mk>
