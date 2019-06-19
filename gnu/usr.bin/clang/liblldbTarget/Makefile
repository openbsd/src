# $OpenBSD: Makefile,v 1.1 2018/10/04 11:43:29 patrick Exp $

LIB=	lldbTarget
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	ABI.cpp \
	CPPLanguageRuntime.cpp \
	ExecutionContext.cpp \
	FileAction.cpp \
	JITLoader.cpp \
	JITLoaderList.cpp \
	InstrumentationRuntime.cpp \
	InstrumentationRuntimeStopInfo.cpp \
	Language.cpp \
	LanguageRuntime.cpp \
	Memory.cpp \
	MemoryHistory.cpp \
	ModuleCache.cpp \
	ObjCLanguageRuntime.cpp \
	OperatingSystem.cpp \
	PathMappingList.cpp \
	Platform.cpp \
	Process.cpp \
	ProcessInfo.cpp \
	ProcessLaunchInfo.cpp \
	Queue.cpp \
	QueueItem.cpp \
	QueueList.cpp \
	RegisterContext.cpp \
	RegisterNumber.cpp \
	SectionLoadHistory.cpp \
	SectionLoadList.cpp \
	StackFrame.cpp \
	StackFrameList.cpp \
	StackID.cpp \
	StopInfo.cpp \
	StructuredDataPlugin.cpp \
	SystemRuntime.cpp \
	Target.cpp \
	TargetList.cpp \
	Thread.cpp \
	ThreadCollection.cpp \
	ThreadList.cpp \
	ThreadPlan.cpp \
	ThreadPlanBase.cpp \
	ThreadPlanCallFunction.cpp \
	ThreadPlanCallFunctionUsingABI.cpp \
	ThreadPlanCallOnFunctionExit.cpp \
	ThreadPlanCallUserExpression.cpp \
	ThreadPlanPython.cpp \
	ThreadPlanRunToAddress.cpp \
	ThreadPlanShouldStopHere.cpp \
	ThreadPlanStepInRange.cpp \
	ThreadPlanStepInstruction.cpp \
	ThreadPlanStepOut.cpp \
	ThreadPlanStepOverBreakpoint.cpp \
	ThreadPlanStepOverRange.cpp \
	ThreadPlanStepRange.cpp \
	ThreadPlanStepThrough.cpp \
	ThreadPlanStepUntil.cpp \
	ThreadPlanTracer.cpp \
	ThreadSpec.cpp \
	UnixSignals.cpp \
	UnwindAssembly.cpp \

.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Target

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
