#	$OpenBSD: Makefile,v 1.2 2002/01/02 18:04:48 art Exp $

CLEANFILES+= in in1 out xx -k xsort linecount fields

REGRESSTARGETS=do-stests

# XXX - should be split up in this makefile, not in the script.
# XXX - script doesn't return non-0 on failure.
do-stests: ${.CURDIR}/stests
	${.CURDIR}/stests

.include <bsd.regress.mk>
