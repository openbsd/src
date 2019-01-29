# $OpenBSD: Makefile,v 1.1 2019/01/29 00:18:23 jsg Exp $

LIB=	LLVMAMDGPUInfo
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/AMDGPU \
		-I${LLVM_SRCS}/lib/Target/AMDGPU

.include <bsd.own.mk>
SRCS=	AMDGPUTargetInfo.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/AMDGPU/TargetInfo

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
