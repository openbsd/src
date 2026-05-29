# $OpenBSD: Makefile,v 1.6 2026/05/29 11:18:31 robert Exp $

LIB=	LLVMTarget
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Target

SRCS+=	RegisterTargetPassConfigCallback.cpp \
	Target.cpp \
	TargetLoweringObjectFile.cpp \
	TargetMachine.cpp \
	TargetMachineC.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target
