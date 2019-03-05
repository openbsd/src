# $OpenBSD: Makefile,v 1.3 2019/03/05 09:44:42 jsg Exp $

PROG=	llc
NOMAN=

SRCS=	llc.cpp

.PATH: ${.CURDIR}/../../../llvm/tools/llc

LLVM_LIBDEPS=

DPADD+= ${.OBJDIR}/../libLLVM/libLLVM.a
LDADD+= ${.OBJDIR}/../libLLVM/libLLVM.a

.include <bsd.prog.mk>
