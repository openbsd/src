# $OpenBSD: Makefile,v 1.1 2016/09/05 10:56:50 pascal Exp $

LIB=	LLVMTarget
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Target

.include <bsd.own.mk>
SRCS=	Target.cpp \
	TargetIntrinsicInfo.cpp \
	TargetLoweringObjectFile.cpp \
	TargetMachine.cpp \
	TargetMachineC.cpp \
	TargetRecip.cpp \
	TargetSubtargetInfo.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
