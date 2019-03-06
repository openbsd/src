# $OpenBSD: Makefile,v 1.4 2019/03/06 14:35:28 jsg Exp $

.include <bsd.own.mk>

PROG=	llc
NOMAN=

SRCS=	llc.cpp

.PATH: ${.CURDIR}/../../../llvm/tools/llc

LLVM_LIBDEPS=	LLVM

.include <bsd.prog.mk>
