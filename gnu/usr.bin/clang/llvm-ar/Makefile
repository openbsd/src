# $OpenBSD: Makefile,v 1.1 2021/11/23 10:30:08 robert Exp $

PROG=	llvm-ar
BINDIR=	/usr/bin

.include <bsd.own.mk>

SRCS=	llvm-ar.cpp

.if ${AR_VERSION:L} == "llvm"
LINKS=  ${BINDIR}/${PROG} ${BINDIR}/ar

maninstall:
	cd ${DESTDIR}${MANDIR}1 && rm -f ar.1 && ln llvm-ar.1 ar.1
.endif

.PATH:	${.CURDIR}/../../../llvm/llvm/tools/llvm-ar

LLVM_LIBDEPS=	LLVM LLVMDlltoolDriver LLVMLibDriver

LDADD+= -L ${.OBJDIR}/../libLLVM -lLLVM

.include <bsd.prog.mk>
