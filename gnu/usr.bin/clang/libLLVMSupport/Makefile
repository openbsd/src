# $OpenBSD: Makefile,v 1.13 2021/12/17 14:55:46 patrick Exp $

LIB=	LLVMSupport
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/ADT     \
	-I${LLVM_SRCS}/include/llvm/Support

.include <bsd.own.mk>

SRCS=	AArch64TargetParser.cpp \
	ABIBreak.cpp \
	ARMTargetParser.cpp \
	AMDGPUMetadata.cpp \
	APFixedPoint.cpp \
	APFloat.cpp \
	APInt.cpp \
	APSInt.cpp \
	ARMBuildAttrs.cpp \
	ARMAttributeParser.cpp \
	ARMWinEH.cpp \
	Allocator.cpp \
	BinaryStreamError.cpp \
	BinaryStreamReader.cpp \
	BinaryStreamRef.cpp \
	BinaryStreamWriter.cpp \
	BlockFrequency.cpp \
	BranchProbability.cpp \
	BuryPointer.cpp \
	CachePruning.cpp \
	circular_raw_ostream.cpp \
	Chrono.cpp \
	COM.cpp \
	CodeGenCoverage.cpp \
	CommandLine.cpp \
	Compression.cpp \
	CRC.cpp \
	ConvertUTF.cpp \
	ConvertUTFWrapper.cpp \
	CrashRecoveryContext.cpp \
	DataExtractor.cpp \
	Debug.cpp \
	DebugCounter.cpp \
	DeltaAlgorithm.cpp \
	DAGDeltaAlgorithm.cpp \
	DJB.cpp \
	ELFAttributeParser.cpp \
	ELFAttributes.cpp \
	SupportError.cpp \
	ErrorHandling.cpp \
	ExtensibleRTTI.cpp \
	FileCollector.cpp \
	FileUtilities.cpp \
	FileOutputBuffer.cpp \
	FoldingSet.cpp \
	FormattedStream.cpp \
	FormatVariadic.cpp \
	GlobPattern.cpp \
	GraphWriter.cpp \
	Hashing.cpp \
	InitLLVM.cpp \
	InstructionCost.cpp \
	IntEqClasses.cpp \
	IntervalMap.cpp \
	ItaniumManglingCanonicalizer.cpp \
	JSON.cpp \
	KnownBits.cpp \
	LEB128.cpp \
	LineIterator.cpp \
	Locale.cpp \
	LockFileManager.cpp \
	SupportLowLevelType.cpp \
	ManagedStatic.cpp \
	MathExtras.cpp \
	MemAlloc.cpp \
	MemoryBuffer.cpp \
	MemoryBufferRef.cpp \
	MD5.cpp \
	NativeFormatting.cpp \
	OptimizedStructLayout.cpp \
	Optional.cpp \
	Parallel.cpp \
	PluginLoader.cpp \
	PrettyStackTrace.cpp \
	RandomNumberGenerator.cpp \
	Regex.cpp \
	RISCVAttributes.cpp \
	RISCVAttributeParser.cpp \
	ScaledNumber.cpp \
	ScopedPrinter.cpp \
	SHA1.cpp \
	Signposts.cpp \
	SmallPtrSet.cpp \
	SmallVector.cpp \
	SourceMgr.cpp \
	SpecialCaseList.cpp \
	Statistic.cpp \
	StringExtras.cpp \
	StringMap.cpp \
	StringSaver.cpp \
	StringRef.cpp \
	SuffixTree.cpp \
	SymbolRemappingReader.cpp \
	SystemUtils.cpp \
	TarWriter.cpp \
	TargetParser.cpp \
	ThreadPool.cpp \
	TimeProfiler.cpp \
	Timer.cpp \
	ToolOutputFile.cpp \
	TrigramIndex.cpp \
	Triple.cpp \
	Twine.cpp \
	TypeSize.cpp \
	Unicode.cpp \
	UnicodeCaseFold.cpp \
	VersionTuple.cpp \
	VirtualFileSystem.cpp \
	WithColor.cpp \
	X86TargetParser.cpp \
	YAMLParser.cpp \
	YAMLTraits.cpp \
	raw_os_ostream.cpp \
	raw_ostream.cpp \
	regcomp.c \
	regerror.c \
	regexec.c \
	regfree.c \
	regstrlcpy.c \
	xxhash.cpp \
	Z3Solver.cpp \
	Atomic.cpp \
	DynamicLibrary.cpp \
	Errno.cpp \
	Host.cpp \
	Memory.cpp \
	Path.cpp \
	Process.cpp \
	Program.cpp \
	RWMutex.cpp \
	Signals.cpp \
	TargetRegistry.cpp \
	ThreadLocal.cpp \
	Threading.cpp \
	Valgrind.cpp \
	Watchdog.cpp

SupportError.cpp:
	ln -s ${LLVM_SRCS}/lib/Support/Error.cpp $@

SupportLowLevelType.cpp:
	ln -s ${LLVM_SRCS}/lib/Support/LowLevelType.cpp $@

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Support

install:
	@# Nothing here so far ...

CLEANFILES+= SupportError.cpp SupportLowLevelType.cpp

.include <bsd.lib.mk>
