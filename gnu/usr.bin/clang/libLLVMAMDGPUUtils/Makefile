# $OpenBSD: Makefile,v 1.2 2019/03/05 09:44:39 jsg Exp $

LIB=	LLVMAMDGPUUtils
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/AMDGPU \
		-I${LLVM_SRCS}/lib/Target/AMDGPU

SRCS+=	AMDGPUBaseInfo.cpp \
	AMDKernelCodeTUtils.cpp \
	AMDGPUAsmUtils.cpp \
	AMDGPULaneDominator.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/AMDGPU/Utils
