# $OpenBSD: Makefile,v 1.1 2018/04/06 14:44:04 patrick Exp $

LIB=	clangBasicTargets
NOPIC=
NOPROFILE=

CPPFLAGS+=	${CLANG_INCLUDES}
CPPFLAGS+=	-I${LLVM_SRCS}/tools/clang/lib/Basic
CPPFLAGS+=	-I${LLVM_SRCS}/tools/clang/lib/Basic/Targets

.include <bsd.own.mk>
SRCS=	AArch64.cpp \
	AMDGPU.cpp \
	ARM.cpp \
	AVR.cpp \
	BPF.cpp \
	Hexagon.cpp \
	Lanai.cpp \
	Le64.cpp \
	MSP430.cpp \
	Mips.cpp \
	NVPTX.cpp \
	Nios2.cpp \
	OSTargets.cpp \
	PNaCl.cpp \
	PPC.cpp \
	SPIR.cpp \
	Sparc.cpp \
	SystemZ.cpp \
	TCE.cpp \
	WebAssembly.cpp \
	X86.cpp \
	XCore.cpp

.PATH:	${.CURDIR}/../../../llvm/tools/clang/lib/Basic/Targets

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
