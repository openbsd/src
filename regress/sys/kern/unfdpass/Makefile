#	$OpenBSD: Makefile,v 1.6 2002/02/11 12:36:58 art Exp $
#	$NetBSD: Makefile,v 1.3 1998/03/02 21:57:38 cgd Exp $

PROG=	unfdpass

CLEANFILES+=file1 file2 output test-sock
REGRESSTARGETS=do-unfdpass

do-unfdpass: ${PROG}
	./unfdpass > output
	cmp -s ${.CURDIR}/expected output

.include <bsd.regress.mk>
