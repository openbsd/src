# $OpenBSD: Makefile,v 1.5 2020/08/03 14:45:27 patrick Exp $

LIB=	LLVMTarget
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Target

SRCS+=	Target.cpp \
	TargetIntrinsicInfo.cpp \
	TargetLoweringObjectFile.cpp \
	TargetMachine.cpp \
	TargetMachineC.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target
