#	$OpenBSD: Makefile.inc,v 1.2 2005/12/27 18:42:24 drahn Exp $

.PATH: ${LIBCSRCDIR}/dlfcn

.include <bsd.own.mk>

.if ${ELF_TOOLCHAIN:L} == "yes"
SRCS+=		dlfcn_stubs.c
.endif
