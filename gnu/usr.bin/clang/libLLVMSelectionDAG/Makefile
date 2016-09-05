# $OpenBSD: Makefile,v 1.1 2016/09/05 10:56:49 pascal Exp $

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
	StatepointLowering.cpp \
	ScheduleDAGVLIW.cpp \
	TargetLowering.cpp \
	TargetSelectionDAGInfo.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/CodeGen/SelectionDAG

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
