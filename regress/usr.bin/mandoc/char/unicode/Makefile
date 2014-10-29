# $OpenBSD: Makefile,v 1.3 2014/10/29 03:39:26 schwarze Exp $

REGRESS_TARGETS  = ascii invalid latin1 latin1diff
REGRESS_TARGETS += man mdoc named namediff nogroff
SKIP_ASCII 	 = man mdoc
UTF8_TARGETS	 = ${REGRESS_TARGETS}
HTML_TARGETS	 = ascii invalid latin1 latin1diff named namediff nogroff
LINT_TARGETS	 = invalid

SKIP_GROFF	 = nogroff
SKIP_GROFF_ASCII = latin1diff namediff

.include <bsd.regress.mk>
