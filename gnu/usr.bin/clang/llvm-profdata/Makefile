# $OpenBSD: Makefile,v 1.3 2026/05/29 11:18:32 robert Exp $

.include <bsd.own.mk>

PROG=	llvm-profdata
BINDIR=	/usr/bin

SRCS+=	llvm-profdata.cpp
MAN+=	llvm-profdata.1

.PATH:	${.CURDIR}/../../../llvm/llvm/tools/llvm-profdata

LLVM_LIBDEPS=	LLVM

LDADD+= -L ${.OBJDIR}/../libLLVM -lLLVM

.include <bsd.prog.mk>
