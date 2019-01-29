# $OpenBSD: Makefile,v 1.1 2019/01/29 00:18:23 jsg Exp $

LIB=	LLVMAMDGPUDisassembler
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/AMDGPU \
		-I${LLVM_SRCS}/lib/Target/AMDGPU

.include <bsd.own.mk>
SRCS=	AMDGPUDisassembler.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/AMDGPU/Disassembler

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
