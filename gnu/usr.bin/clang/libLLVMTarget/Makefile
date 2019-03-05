# $OpenBSD: Makefile,v 1.4 2019/03/05 09:44:42 jsg Exp $

LIB=	LLVMTarget
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Target

SRCS+=	Target.cpp \
	TargetIntrinsicInfo.cpp \
	TargetLoweringObjectFile.cpp \
	TargetMachine.cpp \
	TargetMachineC.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target
