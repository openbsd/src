# $OpenBSD: Makefile,v 1.5 2021/12/17 14:55:46 patrick Exp $

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
	SBFileSpec.cpp \
	SBFile.cpp \
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
	SBReproducer.cpp \
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
	SBType.cpp \
	SBTypeCategory.cpp \
	SBTypeEnumMember.cpp \
	SBTypeFilter.cpp \
	SBTypeFormat.cpp \
	SBTypeNameSpecifier.cpp \
	SBTypeSummary.cpp \
	SBTypeSynthetic.cpp \
	SBValue.cpp \
	SBValueList.cpp \
	SBVariablesOptions.cpp \
	SBWatchpoint.cpp \
	SBUnixSignals.cpp \
	SystemInitializerFull.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/API

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
CPPFLAGS+=	-I${.CURDIR}/../include/lldb/
