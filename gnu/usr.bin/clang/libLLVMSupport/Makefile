# $OpenBSD: Makefile,v 1.10 2019/06/23 22:07:41 patrick Exp $

LIB=	LLVMSupport
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Support

.include <bsd.own.mk>
SRCS=	AArch64TargetParser.cpp \
	AMDGPUMetadata.cpp \
	APFloat.cpp \
	APInt.cpp \
	APSInt.cpp \
	ARMAttributeParser.cpp \
	ARMBuildAttrs.cpp \
	ARMTargetParser.cpp \
	ARMWinEH.cpp \
	Allocator.cpp \
	BinaryStreamError.cpp \
	BinaryStreamReader.cpp \
	BinaryStreamRef.cpp \
	BinaryStreamWriter.cpp \
	BlockFrequency.cpp \
	BranchProbability.cpp \
	BuryPointer.cpp \
	circular_raw_ostream.cpp \
	COM.cpp \
	CachePruning.cpp \
	Chrono.cpp \
	CodeGenCoverage.cpp \
	CommandLine.cpp \
	Compression.cpp \
	ConvertUTF.cpp \
	ConvertUTFWrapper.cpp \
	CrashRecoveryContext.cpp \
	DAGDeltaAlgorithm.cpp \
	DJB.cpp \
	DataExtractor.cpp \
	Debug.cpp \
	DebugCounter.cpp \
	DeltaAlgorithm.cpp \
	SupportError.cpp \
	ErrorHandling.cpp \
	FileCheck.cpp \
	FileUtilities.cpp \
	FileOutputBuffer.cpp \
	FoldingSet.cpp \
	FormatVariadic.cpp \
	FormattedStream.cpp \
	GlobPattern.cpp \
	GraphWriter.cpp \
	Hashing.cpp \
	InitLLVM.cpp \
	IntEqClasses.cpp \
	IntervalMap.cpp \
	ItaniumManglingCanonicalizer.cpp \
	JSON.cpp \
	JamCRC.cpp \
	KnownBits.cpp \
	LEB128.cpp \
	LineIterator.cpp \
	Locale.cpp \
	LockFileManager.cpp \
	SupportLowLevelType.cpp \
	ManagedStatic.cpp \
	MathExtras.cpp \
	MemoryBuffer.cpp \
	MD5.cpp \
	NativeFormatting.cpp \
	Options.cpp \
	Parallel.cpp \
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
	SymbolRemappingReader.cpp \
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
	Signals.cpp \
	TargetRegistry.cpp \
	ThreadLocal.cpp \
	Threading.cpp \
	UnicodeCaseFold.cpp \
	Valgrind.cpp \
	VersionTuple.cpp \
	VirtualFileSystem.cpp \
	Watchdog.cpp \
	WithColor.cpp \
	xxhash.cpp

SupportError.cpp:
	ln -s ${LLVM_SRCS}/lib/Support/Error.cpp $@

SupportLowLevelType.cpp:
	ln -s ${LLVM_SRCS}/lib/Support/LowLevelType.cpp $@

.PATH:	${.CURDIR}/../../../llvm/lib/Support

install:
	@# Nothing here so far ...

CLEANFILES+= SupportError.cpp SupportLowLevelType.cpp

.include <bsd.lib.mk>
