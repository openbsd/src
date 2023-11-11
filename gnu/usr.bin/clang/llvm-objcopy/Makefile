# $OpenBSD: Makefile,v 1.6 2023/11/11 18:35:40 robert Exp $

.include <bsd.own.mk>

PROG=	llvm-objcopy
MAN=	llvm-objcopy.1
BINDIR=	/usr/bin
LIBEXECDIR=/usr/libexec

DRIVER_TEMPLATE=Yes

SRCS+=	ObjcopyOptions.cpp \
	llvm-objcopy.cpp

.if ${LINKER_VERSION:L} == "lld"
LINKS=	${BINDIR}/llvm-objcopy ${BINDIR}/strip
MAN+=	strip.1
.endif

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm-objcopy
CPPFLAGS+=	-I${.CURDIR}/../../../llvm/llvm/tools/llvm-objcopy

.PATH:	${.CURDIR}/../../../llvm/llvm/tools/llvm-objcopy

LLVM_LIBDEPS=	LLVM \
		LLVMObjCopy

LDADD+= -L ${.OBJDIR}/../libLLVM -lLLVM

.include <bsd.prog.mk>
