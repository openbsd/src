# $OpenBSD: Makefile,v 1.1 2018/10/04 11:43:29 patrick Exp $

LIB=	lldbAPI
NOPIC=
NOPROFILE=

.include <bsd.own.mk>

SRCS=	SBAddress.cpp \
	SBAttachInfo.cpp \
	SBBlock.cpp \
	SBBreakpoint.cpp \
	SBBreakpointLocation.cpp \
	SBBreakpointName.cpp \
	SBBreakpointOptionCommon.cpp \
	SBBroadcaster.cpp \
	SBCommandInterpreter.cpp \
	SBCommandReturnObject.cpp \
	SBCommunication.cpp \
	SBCompileUnit.cpp \
	SBData.cpp \
	SBDebugger.cpp \
	SBDeclaration.cpp \
	SBError.cpp \
	SBEvent.cpp \
	SBExecutionContext.cpp \
	SBExpressionOptions.cpp \
	SBFileSpec.cpp \
	SBFileSpecList.cpp \
	SBFrame.cpp \
	SBFunction.cpp \
	SBHostOS.cpp \
	SBInstruction.cpp \
	SBInstructionList.cpp \
	SBLanguageRuntime.cpp \
	SBLaunchInfo.cpp \
	SBLineEntry.cpp \
	SBListener.cpp \
	SBMemoryRegionInfo.cpp \
	SBMemoryRegionInfoList.cpp \
	SBModule.cpp \
	SBModuleSpec.cpp \
	SBPlatform.cpp \
	SBProcess.cpp \
	SBProcessInfo.cpp \
	SBQueue.cpp \
	SBQueueItem.cpp \
	SBSection.cpp \
	SBSourceManager.cpp \
	SBStream.cpp \
	SBStringList.cpp \
	SBStructuredData.cpp \
	SBSymbol.cpp \
	SBSymbolContext.cpp \
	SBSymbolContextList.cpp \
	SBTarget.cpp \
	SBThread.cpp \
	SBThreadCollection.cpp \
	SBThreadPlan.cpp \
	SBTrace.cpp \
	SBTraceOptions.cpp \
	SBType.cpp \
	SBTypeCategory.cpp \
	SBTypeEnumMember.cpp \
	SBTypeFilter.cpp \
	SBTypeFormat.cpp \
	SBTypeNameSpecifier.cpp \
	SBTypeSummary.cpp \
	SBTypeSynthetic.cpp \
	SBUnixSignals.cpp \
	SBValue.cpp \
	SBValueList.cpp \
	SBVariablesOptions.cpp \
	SBWatchpoint.cpp \
	SystemInitializerFull.cpp

.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/API

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
