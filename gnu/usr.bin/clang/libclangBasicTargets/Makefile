# $OpenBSD: Makefile,v 1.5 2023/11/11 18:35:38 robert Exp $

LIB=	clangBasicTargets
NOPIC=
NOPROFILE=

CPPFLAGS+=	${CLANG_INCLUDES}
CPPFLAGS+=	-I${CLANG_SRCS}/lib/Basic
CPPFLAGS+=	-I${CLANG_SRCS}/lib/Basic/Targets

.include <bsd.own.mk>

SRCS=	AArch64.cpp \
	AMDGPU.cpp \
	ARC.cpp \
	ARM.cpp \
	AVR.cpp \
	BPF.cpp \
	CSKY.cpp \
	DirectX.cpp \
	Hexagon.cpp \
	Lanai.cpp \
	Le64.cpp \
	LoongArch.cpp \
	M68k.cpp \
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
	VE.cpp \
	WebAssembly.cpp \
	X86.cpp \
	XCore.cpp

.PATH:	${.CURDIR}/../../../llvm/clang/lib/Basic/Targets

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
