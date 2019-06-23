# $OpenBSD: Makefile,v 1.3 2019/06/23 22:07:42 patrick Exp $

LIB=	clangBasicTargets
NOPIC=
NOPROFILE=

CPPFLAGS+=	${CLANG_INCLUDES}
CPPFLAGS+=	-I${LLVM_SRCS}/tools/clang/lib/Basic
CPPFLAGS+=	-I${LLVM_SRCS}/tools/clang/lib/Basic/Targets

.include <bsd.own.mk>
SRCS=	AArch64.cpp \
	AMDGPU.cpp \
	ARC.cpp \
	ARM.cpp \
	AVR.cpp \
	BPF.cpp \
	Hexagon.cpp \
	Lanai.cpp \
	Le64.cpp \
	MSP430.cpp \
	Mips.cpp \
	NVPTX.cpp \
	OSTargets.cpp \
	PNaCl.cpp \
	PPC.cpp \
	RISCV.cpp \
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
