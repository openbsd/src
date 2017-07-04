#	$OpenBSD: Makefile,v 1.7 2017/07/04 21:56:05 bluhm Exp $
#
# Author: Ingo Schwarze <schwarze@openbsd.org>, 2016.  Public Domain.

REGRESS_TARGETS = getcmd read_char

CLEANFILES +=	test_read_char test_getcmd out_getcmd.txt

read_char: generated test_read_char
	sh ${.CURDIR}/test_read_char.sh

getcmd: generated test_getcmd
	echo -n s | ./test_getcmd > out_getcmd.txt
	diff -u ${.CURDIR}/exp_getcmd.txt out_getcmd.txt

.include <bsd.regress.mk>
