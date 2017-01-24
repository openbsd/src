# $OpenBSD: Makefile,v 1.3 2017/01/24 08:44:47 patrick Exp $

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
	Chrono.cpp \
	CommandLine.cpp \
	Compression.cpp \
	ConvertUTF.cpp \
	ConvertUTFWrapper.cpp \
	CrashRecoveryContext.cpp \
	DataExtractor.cpp \
	Debug.cpp \
	DeltaAlgorithm.cpp \
	DAGDeltaAlgorithm.cpp \
	Dwarf.cpp \
	Error.cpp \
	ErrorHandling.cpp \
	FileUtilities.cpp \
	FileOutputBuffer.cpp \
	FoldingSet.cpp \
	FormattedStream.cpp \
	GlobPattern.cpp \
	GraphWriter.cpp \
	Hashing.cpp \
	IntEqClasses.cpp \
	IntervalMap.cpp \
	JamCRC.cpp \
	LEB128.cpp \
	LineIterator.cpp \
	Locale.cpp \
	LockFileManager.cpp \
	ManagedStatic.cpp \
	MathExtras.cpp \
	MemoryBuffer.cpp \
	MD5.cpp \
	NativeFormatting.cpp \
	Options.cpp \
	PluginLoader.cpp \
	PrettyStackTrace.cpp \
	RandomNumberGenerator.cpp \
	Regex.cpp \
	ScaledNumber.cpp \
	ScopedPrinter.cpp \
	SHA1.cpp \
	SmallPtrSet.cpp \
	SmallVector.cpp \
	SourceMgr.cpp \
	SpecialCaseList.cpp \
	Statistic.cpp \
	StringExtras.cpp \
	StringMap.cpp \
	StringPool.cpp \
	StringSaver.cpp \
	StringRef.cpp \
	SystemUtils.cpp \
	TarWriter.cpp \
	TargetParser.cpp \
	ThreadPool.cpp \
	Timer.cpp \
	ToolOutputFile.cpp \
	TrigramIndex.cpp \
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
	Valgrind.cpp \
	Watchdog.cpp \
	xxhash.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Support

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
