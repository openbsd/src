#	$OpenBSD: Makefile,v 1.1 2001/01/12 21:06:17 millert Exp $

NOMAN=
NOPROG=

CLEANFILES+= in in1 out xx -k xsort linecount fields

regress: ${.CURDIR}/stests
	${.CURDIR}/stests

.include <bsd.prog.mk>
