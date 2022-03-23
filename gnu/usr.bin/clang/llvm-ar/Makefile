# $OpenBSD: Makefile,v 1.3 2022/03/23 14:23:44 robert Exp $

PROG=	ar
BINDIR=	/usr/bin

.include <bsd.own.mk>

SRCS=	llvm-ar.cpp

LINKS=	${BINDIR}/ar ${BINDIR}/ranlib
MAN+=	ranlib.1

.PATH:	${.CURDIR}/../../../llvm/llvm/tools/llvm-ar

LLVM_LIBDEPS=	LLVM LLVMDlltoolDriver LLVMLibDriver

LDADD+= -L ${.OBJDIR}/../libLLVM -lLLVM

.include <bsd.prog.mk>
