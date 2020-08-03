# $OpenBSD: Makefile,v 1.3 2020/08/03 14:45:24 patrick Exp $

LIB=	LLVMAMDGPUInfo
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/AMDGPU \
		-I${LLVM_SRCS}/lib/Target/AMDGPU

SRCS+=	AMDGPUTargetInfo.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/AMDGPU/TargetInfo
