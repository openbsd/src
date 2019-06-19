# $OpenBSD: Makefile,v 1.1 2019/01/27 16:59:42 patrick Exp $

LIB=	clangDriverArch
NOPIC=
NOPROFILE=

CPPFLAGS+=	${CLANG_INCLUDES}
CPPFLAGS+=	-I${LLVM_SRCS}/tools/clang/lib/Driver

.include <bsd.own.mk>
SRCS=	AArch64.cpp \
	ARM.cpp \
	Mips.cpp \
	PPC.cpp \
	RISCV.cpp \
	Sparc.cpp \
	SystemZ.cpp \
	X86.cpp

.PATH:	${.CURDIR}/../../../llvm/tools/clang/lib/Driver/ToolChains/Arch

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
