#	$OpenBSD: Makefile.inc,v 1.3 2013/07/05 21:29:51 miod Exp $

.PATH: ${LIBCSRCDIR}/dlfcn

.include <bsd.own.mk>

SRCS+=		dlfcn_stubs.c
