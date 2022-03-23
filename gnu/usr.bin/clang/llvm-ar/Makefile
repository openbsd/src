# $OpenBSD: Makefile,v 1.4 2022/03/23 22:44:01 deraadt Exp $

PROG=	ar
BINDIR=	/usr/bin

.include <bsd.own.mk>

SRCS=	llvm-ar.cpp

LINKS=	${BINDIR}/ar ${BINDIR}/ranlib
MAN+=	ranlib.1 ar.1

.PATH:	${.CURDIR}/../../../llvm/llvm/tools/llvm-ar

LLVM_LIBDEPS=	LLVM LLVMDlltoolDriver LLVMLibDriver

LDADD+= -L ${.OBJDIR}/../libLLVM -lLLVM

.include <bsd.prog.mk>
