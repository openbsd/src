#	$OpenBSD: Makefile,v 1.11 2017/01/26 04:45:46 benno Exp $
#	$NetBSD: Makefile,v 1.3 1998/03/02 21:57:38 cgd Exp $

PROG=		unfdpass
WARNINGS=	Yes

CLEANFILES+=file1 file2 file3 output test-sock
REGRESS_TARGETS=do-unfdpass1 do-unfdpass2 do-unfdpass3 do-unfdpass4

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

.include <bsd.regress.mk>
