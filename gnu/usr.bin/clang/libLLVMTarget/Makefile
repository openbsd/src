# $OpenBSD: Makefile,v 1.3 2017/07/09 15:28:35 espie Exp $

LIB=	LLVMTarget
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Target

.include <bsd.own.mk>
SRCS=	Target.cpp \
	TargetIntrinsicInfo.cpp \
	TargetLoweringObjectFile.cpp \
	TargetMachine.cpp \
	TargetMachineC.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
