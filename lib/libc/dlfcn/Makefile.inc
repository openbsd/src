#	$OpenBSD: Makefile.inc,v 1.4 2016/03/20 02:32:39 guenther Exp $

.PATH: ${LIBCSRCDIR}/dlfcn

.include <bsd.own.mk>

SRCS+=		dlfcn_stubs.c init.c
