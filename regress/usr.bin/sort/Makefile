#	$OpenBSD: Makefile,v 1.4 2003/03/09 06:00:58 david Exp $

CLEANFILES+= in in1 out xx -k xsort linecount fields

REGRESS_TARGETS=do-stests

# XXX - should be split up in this makefile, not in the script.
# XXX - script doesn't return non-0 on failure.
do-stests: ${.CURDIR}/stests
	sh ${.CURDIR}/stests

.include <bsd.regress.mk>
