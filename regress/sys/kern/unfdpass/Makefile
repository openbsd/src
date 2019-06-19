#	$OpenBSD: Makefile,v 1.12 2018/11/28 08:06:22 claudio Exp $
#	$NetBSD: Makefile,v 1.3 1998/03/02 21:57:38 cgd Exp $

PROG=		unfdpass
WARNINGS=	Yes

CLEANFILES+=file1 file2 file3 file4 file5 output test-sock
REGRESS_TARGETS=do-unfdpass1 do-unfdpass2 do-unfdpass3 do-unfdpass4
REGRESS_TARGETS+=do-unfdpass5 do-unfdpass6 do-unfdpass7 do-unfdpass8
REGRESS_TARGETS+=do-unfdpass9 do-unfdpass10 do-unfdpass11 do-unfdpass12

do-unfdpass1: ${PROG}
	./unfdpass > output
	cmp -s ${.CURDIR}/expected output

do-unfdpass2: ${PROG}
	./unfdpass -p > output
	cmp -s ${.CURDIR}/expected output

do-unfdpass3: ${PROG}
	./unfdpass -q > output
	cmp -s ${.CURDIR}/expected output

do-unfdpass4: ${PROG}
	./unfdpass -pq > output
	cmp -s ${.CURDIR}/expected output

do-unfdpass5: ${PROG}
	./unfdpass -o > output
	cmp -s ${.CURDIR}/expected_overflow output

do-unfdpass6: ${PROG}
	./unfdpass -op > output
	cmp -s ${.CURDIR}/expected_overflow output

do-unfdpass7: ${PROG}
	./unfdpass -oq > output
	cmp -s ${.CURDIR}/expected_overflow output

do-unfdpass8: ${PROG}
	./unfdpass -opq > output
	cmp -s ${.CURDIR}/expected_overflow output

do-unfdpass9: ${PROG}
	./unfdpass -r > output
	cmp -s ${.CURDIR}/expected_read output

do-unfdpass10: ${PROG}
	./unfdpass -rp > output
	cmp -s ${.CURDIR}/expected_read output

do-unfdpass11: ${PROG}
	./unfdpass -rq > output
	cmp -s ${.CURDIR}/expected_read output

do-unfdpass12: ${PROG}
	./unfdpass -rpq > output
	cmp -s ${.CURDIR}/expected_read output

.include <bsd.regress.mk>
