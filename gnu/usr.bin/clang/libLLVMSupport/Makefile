# $OpenBSD: Makefile,v 1.14 2023/11/11 18:35:38 robert Exp $

LIB=	LLVMSupport
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/ADT \
		-I${LLVM_SRCS}/include/llvm/Support \
		-DBLAKE3_NO_AVX512 -DBLAKE3_NO_AVX2 \
		-DBLAKE3_NO_SSE41 -DBLAKE3_NO_SSE2

.include <bsd.own.mk>

SRCS+=	ABIBreak.cpp \
	AMDGPUMetadata.cpp \
	APFixedPoint.cpp \
	APFloat.cpp \
	APInt.cpp \
	APSInt.cpp \
	ARMAttributeParser.cpp \
	ARMBuildAttrs.cpp \
	ARMWinEH.cpp \
	AddressRanges.cpp \
	Allocator.cpp \
	Atomic.cpp \
	AutoConvert.cpp \
	Base64.cpp \
	BinaryStreamError.cpp \
	BinaryStreamReader.cpp \
	BinaryStreamRef.cpp \
	BinaryStreamWriter.cpp \
	BlockFrequency.cpp \
	BranchProbability.cpp \
	BuryPointer.cpp \
	COM.cpp \
	CRC.cpp \
	CSKYAttributeParser.cpp \
	CSKYAttributes.cpp \
	CachePruning.cpp \
	Caching.cpp \
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
	DivisionByConstantInfo.cpp \
	DynamicLibrary.cpp \
	ELFAttributeParser.cpp \
	ELFAttributes.cpp \
	Errno.cpp \
	SupportError.cpp \
	ErrorHandling.cpp \
	ExtensibleRTTI.cpp \
	FileCollector.cpp \
	FileOutputBuffer.cpp \
	FileUtilities.cpp \
	FoldingSet.cpp \
	FormatVariadic.cpp \
	FormattedStream.cpp \
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
	MD5.cpp \
	MSP430AttributeParser.cpp \
	MSP430Attributes.cpp \
	ManagedStatic.cpp \
	MathExtras.cpp \
	MemAlloc.cpp \
	Memory.cpp \
	MemoryBuffer.cpp \
	MemoryBufferRef.cpp \
	NativeFormatting.cpp \
	OptimizedStructLayout.cpp \
	Optional.cpp \
	Parallel.cpp \
	Path.cpp \
	PluginLoader.cpp \
	PrettyStackTrace.cpp \
	Process.cpp \
	Program.cpp \
	RISCVAttributeParser.cpp \
	RISCVAttributes.cpp \
	RISCVISAInfo.cpp \
	RWMutex.cpp \
	RandomNumberGenerator.cpp \
	Regex.cpp \
	SHA1.cpp \
	SHA256.cpp \
	ScaledNumber.cpp \
	ScopedPrinter.cpp \
	Signals.cpp \
	Signposts.cpp \
	SmallPtrSet.cpp \
	SmallVector.cpp \
	SourceMgr.cpp \
	SpecialCaseList.cpp \
	Statistic.cpp \
	StringExtras.cpp \
	StringMap.cpp \
	StringRef.cpp \
	StringSaver.cpp \
	SuffixTree.cpp \
	SymbolRemappingReader.cpp \
	SystemUtils.cpp \
	TarWriter.cpp \
	ThreadPool.cpp \
	Threading.cpp \
	TimeProfiler.cpp \
	Timer.cpp \
	ToolOutputFile.cpp \
	TrigramIndex.cpp \
	Twine.cpp \
	TypeSize.cpp \
	Unicode.cpp \
	UnicodeCaseFold.cpp \
	UnicodeNameToCodepoint.cpp \
	UnicodeNameToCodepointGenerated.cpp \
	Valgrind.cpp \
	VersionTuple.cpp \
	VirtualFileSystem.cpp \
	Watchdog.cpp \
	WithColor.cpp \
	YAMLParser.cpp \
	YAMLTraits.cpp \
	Z3Solver.cpp \
	circular_raw_ostream.cpp \
	raw_os_ostream.cpp \
	raw_ostream.cpp \
	regcomp.c \
	regerror.c \
	regexec.c \
	regfree.c \
	regstrlcpy.c \
	xxhash.cpp

# BLAKE3
SRCS+=	blake3.c \
	blake3_dispatch.c \
	blake3_portable.c \
	blake3_neon.c

SupportError.cpp:
	ln -s ${LLVM_SRCS}/lib/Support/Error.cpp $@

SupportLowLevelType.cpp:
	ln -s ${LLVM_SRCS}/lib/Support/LowLevelType.cpp $@

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Support
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Support/BLAKE3

install:
	@# Nothing here so far ...

CLEANFILES+= SupportError.cpp SupportLowLevelType.cpp

.include <bsd.lib.mk>
