# $OpenBSD: Makefile,v 1.7 2021/04/28 12:55:40 patrick Exp $

LIB=	LLVMSelectionDAG
NOPROFILE=

SRCS+=	DAGCombiner.cpp \
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
	ScheduleDAGVLIW.cpp \
	SelectionDAGBuilder.cpp \
	SelectionDAG.cpp \
	SelectionDAGAddressAnalysis.cpp \
	SelectionDAGDumper.cpp \
	SelectionDAGISel.cpp \
	SelectionDAGPrinter.cpp \
	SelectionDAGTargetInfo.cpp \
	StatepointLowering.cpp \
	TargetLowering.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/CodeGen/SelectionDAG
