# $OpenBSD: Makefile,v 1.8 2026/05/29 11:18:31 robert Exp $

LIB=	lldbAPI
NOPIC=
NOPROFILE=

.include <bsd.own.mk>

SRCS=	SBAddress.cpp \
	SBAddressRange.cpp \
	SBAddressRangeList.cpp \
	SBAttachInfo.cpp \
	SBBlock.cpp \
	SBBreakpoint.cpp \
	SBBreakpointLocation.cpp \
	SBBreakpointName.cpp \
	SBBreakpointOptionCommon.cpp \
	SBBroadcaster.cpp \
	SBCommandInterpreter.cpp \
	SBCommandInterpreterRunOptions.cpp \
	SBCommandReturnObject.cpp \
	SBCommunication.cpp \
	SBCompileUnit.cpp \
	SBData.cpp \
	SBDebugger.cpp \
	SBDeclaration.cpp \
	SBEnvironment.cpp \
	SBError.cpp \
	SBEvent.cpp \
	SBExecutionContext.cpp \
	SBExpressionOptions.cpp \
	SBFile.cpp \
	SBFileSpec.cpp \
	SBFileSpecList.cpp \
	SBFormat.cpp \
	SBFrame.cpp \
	SBFrameList.cpp \
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
	SBMutex.cpp \
	SBPlatform.cpp \
	SBProcess.cpp \
	SBProcessInfo.cpp \
	SBProcessInfoList.cpp \
	SBProgress.cpp \
	SBQueue.cpp \
	SBQueueItem.cpp \
	SBReproducer.cpp \
	SBSaveCoreOptions.cpp \
	SBScriptObject.cpp \
	SBSection.cpp \
	SBSourceManager.cpp \
	SBStatisticsOptions.cpp \
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
	SBTraceCursor.cpp \
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
	SBWatchpointOptions.cpp \
	SystemInitializerFull.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/API

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
CPPFLAGS+=	-I${.CURDIR}/../include/lldb/
