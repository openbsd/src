# $OpenBSD: Makefile,v 1.1 2016/09/05 10:56:49 pascal Exp $

LIB=	LLVMSupport
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Support

.include <bsd.own.mk>
SRCS=	APFloat.cpp \
	APInt.cpp \
	APSInt.cpp \
	ARMBuildAttrs.cpp \
	ARMWinEH.cpp \
	Allocator.cpp \
	BlockFrequency.cpp \
	BranchProbability.cpp \
	circular_raw_ostream.cpp \
	COM.cpp \
	CommandLine.cpp \
	Compression.cpp \
	ConvertUTF.c \
	ConvertUTFWrapper.cpp \
	CrashRecoveryContext.cpp \
	DataExtractor.cpp \
	DataStream.cpp \
	Debug.cpp \
	DeltaAlgorithm.cpp \
	DAGDeltaAlgorithm.cpp \
	Dwarf.cpp \
	ErrorHandling.cpp \
	FileUtilities.cpp \
	FileOutputBuffer.cpp \
	FoldingSet.cpp \
	FormattedStream.cpp \
	GraphWriter.cpp \
	Hashing.cpp \
	IntEqClasses.cpp \
	IntervalMap.cpp \
	IntrusiveRefCntPtr.cpp \
	JamCRC.cpp \
	LEB128.cpp \
	LineIterator.cpp \
	Locale.cpp \
	LockFileManager.cpp \
	ManagedStatic.cpp \
	MathExtras.cpp \
	MemoryBuffer.cpp \
	MemoryObject.cpp \
	MD5.cpp \
	Options.cpp \
	PluginLoader.cpp \
	PrettyStackTrace.cpp \
	RandomNumberGenerator.cpp \
	Regex.cpp \
	ScaledNumber.cpp \
	SmallPtrSet.cpp \
	SmallVector.cpp \
	SourceMgr.cpp \
	SpecialCaseList.cpp \
	Statistic.cpp \
	StreamingMemoryObject.cpp \
	StringExtras.cpp \
	StringMap.cpp \
	StringPool.cpp \
	StringSaver.cpp \
	StringRef.cpp \
	SystemUtils.cpp \
	TargetParser.cpp \
	ThreadPool.cpp \
	Timer.cpp \
	ToolOutputFile.cpp \
	Triple.cpp \
	Twine.cpp \
	Unicode.cpp \
	YAMLParser.cpp \
	YAMLTraits.cpp \
	raw_os_ostream.cpp \
	raw_ostream.cpp \
	regcomp.c \
	regerror.c \
	regexec.c \
	regfree.c \
	regstrlcpy.c \
	Atomic.cpp \
	DynamicLibrary.cpp \
	Errno.cpp \
	Host.cpp \
	Memory.cpp \
	Mutex.cpp \
	Path.cpp \
	Process.cpp \
	Program.cpp \
	RWMutex.cpp \
	SearchForAddressOfSpecialSymbol.cpp \
	Signals.cpp \
	TargetRegistry.cpp \
	ThreadLocal.cpp \
	Threading.cpp \
	TimeValue.cpp \
	Valgrind.cpp \
	Watchdog.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Support

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
