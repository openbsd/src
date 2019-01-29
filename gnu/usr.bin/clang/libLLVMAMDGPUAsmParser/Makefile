# $OpenBSD: Makefile,v 1.1 2019/01/29 00:18:23 jsg Exp $

LIB=	LLVMAMDGPUAsmParser
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/AMDGPU \
		-I${LLVM_SRCS}/lib/Target/AMDGPU

.include <bsd.own.mk>
SRCS=	AMDGPUAsmParser.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/AMDGPU/AsmParser

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
