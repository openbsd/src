# $OpenBSD: Makefile,v 1.5 2023/11/11 18:35:40 robert Exp $

PROG=	ar
BINDIR=	/usr/bin

.include <bsd.own.mk>

DRIVER_NAME=	llvm_ar
DRIVER_TEMPLATE=Yes

SRCS+=	llvm-ar.cpp

LINKS=	${BINDIR}/ar ${BINDIR}/ranlib
MAN+=	ranlib.1 ar.1

.PATH:	${.CURDIR}/../../../llvm/llvm/tools/llvm-ar

LLVM_LIBDEPS=	LLVM \
		LLVMDlltoolDriver \
		LLVMLibDriver

LDADD+= -L ${.OBJDIR}/../libLLVM -lLLVM

.include <bsd.prog.mk>
