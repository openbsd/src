# $OpenBSD: Makefile,v 1.3 2017/07/09 15:28:35 espie Exp $

LIB=	LLVMSelectionDAG
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	DAGCombiner.cpp \
	FastISel.cpp \
	FunctionLoweringInfo.cpp \
	InstrEmitter.cpp \
	LegalizeDAG.cpp \
	LegalizeFloatTypes.cpp \
	LegalizeIntegerTypes.cpp \
	LegalizeTypes.cpp \
	LegalizeTypesGeneric.cpp \
	LegalizeVectorOps.cpp \
	LegalizeVectorTypes.cpp \
	ResourcePriorityQueue.cpp \
	ScheduleDAGFast.cpp \
	ScheduleDAGRRList.cpp \
	ScheduleDAGSDNodes.cpp \
	SelectionDAG.cpp \
	SelectionDAGBuilder.cpp \
	SelectionDAGDumper.cpp \
	SelectionDAGISel.cpp \
	SelectionDAGPrinter.cpp \
	SelectionDAGTargetInfo.cpp \
	StatepointLowering.cpp \
	ScheduleDAGVLIW.cpp \
	TargetLowering.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/CodeGen/SelectionDAG

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
