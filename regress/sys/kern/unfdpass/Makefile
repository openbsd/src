#	$OpenBSD: Makefile,v 1.4 2002/01/03 03:15:04 art Exp $
#	$NetBSD: Makefile,v 1.3 1998/03/02 21:57:38 cgd Exp $

PROG=	unfdpass

CLEANFILES+=file1 file2 output test-sock
REGRESSTARGETS=do-unfdpass

do-unfdpass: ${PROG}
	./unfdpass > output
	cmp -s ${.CURDIR}/expected output

.include <bsd.regress.mk>
