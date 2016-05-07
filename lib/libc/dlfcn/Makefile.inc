#	$OpenBSD: Makefile.inc,v 1.5 2016/05/07 19:05:22 guenther Exp $

.PATH: ${LIBCSRCDIR}/dlfcn

.include <bsd.own.mk>

SRCS+=		dlfcn_stubs.c init.c tib.c
