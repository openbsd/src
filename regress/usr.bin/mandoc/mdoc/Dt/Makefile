# $OpenBSD: Makefile,v 1.3 2014/07/02 08:03:57 schwarze Exp $

REGRESS_TARGETS	 = case badsec badarch nodate baddate
REGRESS_TARGETS	+= noos order dupe nobody lateos
LINT_TARGETS	 = case badsec badarch nodate baddate
LINT_TARGETS	+= noos order dupe nobody lateos

SKIP_GROFF	 = badsec badarch nodate baddate noos nobody
SKIP_ASCII	 = badsec badarch nodate baddate noos

.include <bsd.regress.mk>
