#	$NetBSD: Makefile,v 1.3 1998/03/02 21:57:38 cgd Exp $

PROG=	unfdpass
NOMAN=	noman, no way, man

CLEANFILES+=file1 file2 output test-sock

regress:
	@./unfdpass > output
	@if ! cmp -s ${.CURDIR}/expected output; then \
		echo "FAILED"; \
	else \
		echo "PASSED"; \
	fi

.include <bsd.prog.mk>
