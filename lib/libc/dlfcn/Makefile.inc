#	$OpenBSD: Makefile.inc,v 1.1 2001/11/20 01:09:38 pvalchev Exp $

.PATH: ${LIBCSRCDIR}/dlfcn

.include <bsd.own.mk>

.if ${ELF_TOOLCHAIN:L} == "yes"
SRCS+=		dlfcn_stubs.c
CPPFLAGS+=	-I${.CURDIR}/dlfcn
.endif
