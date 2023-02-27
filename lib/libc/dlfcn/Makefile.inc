#	$OpenBSD: Makefile.inc,v 1.6 2023/02/27 15:00:17 deraadt Exp $

.PATH: ${LIBCSRCDIR}/dlfcn

.include <bsd.own.mk>

SRCS+=		dlfcn_stubs.c init.c tib.c

init.o init.po:	_execvesize.c
