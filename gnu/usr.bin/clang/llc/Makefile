# $OpenBSD: Makefile,v 1.5 2020/08/03 14:45:31 patrick Exp $

.include <bsd.own.mk>

PROG=	llc
NOMAN=

SRCS=	llc.cpp

.PATH: ${.CURDIR}/../../../llvm/llvm/tools/llc

LLVM_LIBDEPS=	LLVM

.include <bsd.prog.mk>
