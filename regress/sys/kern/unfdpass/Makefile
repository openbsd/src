#	$OpenBSD: Makefile,v 1.9 2010/06/21 16:55:52 phessler Exp $
#	$NetBSD: Makefile,v 1.3 1998/03/02 21:57:38 cgd Exp $

PROG=	unfdpass

CLEANFILES+=file1 file2 file3 output test-sock
REGRESS_TARGETS=do-unfdpass1 do-unfdpass2

do-unfdpass1: ${PROG}
	./unfdpass > output
	cmp -s ${.CURDIR}/expected output

do-unfdpass2: ${PROG}
	./unfdpass -p > output
	cmp -s ${.CURDIR}/expected output

.include <bsd.regress.mk>
