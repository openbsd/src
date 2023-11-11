# $OpenBSD: Makefile,v 1.2 2023/11/11 18:35:40 robert Exp $

.include <bsd.own.mk>

PROG=	llvm-profdata
BINDIR=	/usr/bin

DRIVER_TEMPLATE=Yes

SRCS+=	llvm-profdata.cpp
MAN+=	llvm-profdata.1

.PATH:	${.CURDIR}/../../../llvm/llvm/tools/llvm-profdata

LLVM_LIBDEPS=	LLVM

LDADD+= -L ${.OBJDIR}/../libLLVM -lLLVM

.include <bsd.prog.mk>
