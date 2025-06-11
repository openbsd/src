# $OpenBSD: Makefile,v 1.3 2025/06/11 17:46:32 robert Exp $

.include <bsd.own.mk>

LLVM_V=		19
LIBDIR=		/usr/lib/clang/${LLVM_V}/lib

.if ${COMPILER_VERSION:L} != "clang"
CC=		clang
CXX=		clang++
.endif

.if ${BUILD_CLANG:L} == "yes"

LIB=	clang_rt.ubsan_minimal
NOPIC=
NOPROFILE=

CFLAGS+=	-fPIC
CPPFLAGS+=	-I${BSDSRCDIR}/gnu/llvm/compiler-rt/lib

.PATH: ${.CURDIR}/../../../llvm/compiler-rt/lib/ubsan_minimal

SRCS+=		ubsan_minimal_handlers.cpp

beforeinstall:
	${INSTALL} -d -o ${BINOWN} -g ${BINGRP} -m ${DIRMODE} \
		${DESTDIR}${LIBDIR}

.include <bsd.lib.mk>

.else
NOPROG=
.include <bsd.prog.mk>
.endif
