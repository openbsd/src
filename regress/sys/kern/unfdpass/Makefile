#	$OpenBSD: Makefile,v 1.7 2002/09/02 20:01:44 avsm Exp $
#	$NetBSD: Makefile,v 1.3 1998/03/02 21:57:38 cgd Exp $

PROG=	unfdpass

CLEANFILES+=file1 file2 output test-sock
REGRESS_TARGETS=do-unfdpass

do-unfdpass: ${PROG}
	./unfdpass > output
	cmp -s ${.CURDIR}/expected output

.include <bsd.regress.mk>
