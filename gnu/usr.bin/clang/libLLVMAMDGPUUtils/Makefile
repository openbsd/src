# $OpenBSD: Makefile,v 1.3 2019/06/23 22:07:39 patrick Exp $

LIB=	LLVMAMDGPUUtils
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/AMDGPU \
		-I${LLVM_SRCS}/lib/Target/AMDGPU

SRCS+=	AMDGPUBaseInfo.cpp \
	AMDKernelCodeTUtils.cpp \
	AMDGPUAsmUtils.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/AMDGPU/Utils
