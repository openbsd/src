# $OpenBSD: Makefile,v 1.8 2026/05/29 11:18:31 robert Exp $

LIB=	lldbTarget
NOPIC=
NOPROFILE=

.include <bsd.own.mk>

SRCS=	ABI.cpp \
	AssertFrameRecognizer.cpp \
	DynamicRegisterInfo.cpp \
	CoreFileMemoryRanges.cpp \
	ExecutionContext.cpp \
	InstrumentationRuntime.cpp \
	InstrumentationRuntimeStopInfo.cpp \
	JITLoader.cpp \
	JITLoaderList.cpp \
	Language.cpp \
	LanguageRuntime.cpp \
	Memory.cpp \
	MemoryHistory.cpp \
	MemoryRegionInfo.cpp \
	MemoryTagMap.cpp \
	ModuleCache.cpp \
	OperatingSystem.cpp \
	PathMappingList.cpp \
	Platform.cpp \
	Process.cpp \
	ProcessTrace.cpp \
	Queue.cpp \
	QueueItem.cpp \
	QueueList.cpp \
	RegisterContext.cpp \
	RegisterContextUnwind.cpp \
	RegisterFlags.cpp \
	RegisterNumber.cpp \
	RemoteAwarePlatform.cpp \
	ScriptedThreadPlan.cpp \
	SyntheticFrameProvider.cpp \
	SectionLoadHistory.cpp \
	SectionLoadList.cpp \
	BorrowedStackFrame.cpp \
	StackFrame.cpp \
	StackFrameList.cpp \
	StackFrameRecognizer.cpp \
	StackID.cpp \
	Statistics.cpp \
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
	ThreadPlanRunToAddress.cpp \
	ThreadPlanSingleThreadTimeout.cpp \
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
	ThreadPlanStack.cpp \
	ThreadSpec.cpp \
	Trace.cpp \
	TraceCursor.cpp \
	TraceExporter.cpp \
	TraceDumper.cpp \
	UnixSignals.cpp \
	UnwindAssembly.cpp \
	UnwindLLDB.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Target

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
CPPFLAGS+=	-I${.OBJDIR}/../include/lldb/Target
