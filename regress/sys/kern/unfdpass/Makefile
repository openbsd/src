#	$OpenBSD: Makefile,v 1.8 2004/04/27 18:51:17 millert Exp $
#	$NetBSD: Makefile,v 1.3 1998/03/02 21:57:38 cgd Exp $

PROG=	unfdpass

CLEANFILES+=file1 file2 output test-sock
REGRESS_TARGETS=do-unfdpass1 do-unfdpass2

do-unfdpass1: ${PROG}
	./unfdpass > output
	cmp -s ${.CURDIR}/expected output

do-unfdpass2: ${PROG}
	./unfdpass -p > output
	cmp -s ${.CURDIR}/expected output

.include <bsd.regress.mk>
