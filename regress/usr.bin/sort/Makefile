#	$OpenBSD: Makefile,v 1.3 2002/09/02 20:01:44 avsm Exp $

CLEANFILES+= in in1 out xx -k xsort linecount fields

REGRESS_TARGETS=do-stests

# XXX - should be split up in this makefile, not in the script.
# XXX - script doesn't return non-0 on failure.
do-stests: ${.CURDIR}/stests
	${.CURDIR}/stests

.include <bsd.regress.mk>
