# $OpenBSD: Makefile,v 1.16 2026/05/29 11:18:31 robert Exp $

LIB=	LLVMSupport
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/ADT \
		-I${LLVM_SRCS}/include/llvm/Support \
		-I${LLVM_SRCS}/../siphash/include \
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
	ARMBuildAttributes.cpp \
	AArch64AttributeParser.cpp \
	AArch64BuildAttributes.cpp \
	ARMWinEH.cpp \
	SupportAllocToken.cpp \
	Allocator.cpp \
	SupportAtomic.cpp \
	AutoConvert.cpp \
	BalancedPartitioning.cpp \
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
	ConvertEBCDIC.cpp \
	ConvertUTFWrapper.cpp \
	CrashRecoveryContext.cpp \
	DAGDeltaAlgorithm.cpp \
	DJB.cpp \
	DXILABI.cpp \
	DynamicAPInt.cpp \
	DataExtractor.cpp \
	Debug.cpp \
	DebugCounter.cpp \
	DeltaAlgorithm.cpp \
	DeltaTree.cpp \
	DivisionByConstantInfo.cpp \
	DynamicLibrary.cpp \
	ELFAttributes.cpp \
	ELFAttrParserCompact.cpp \
	ELFAttrParserExtended.cpp \
	Errno.cpp \
	SupportError.cpp \
	ErrorHandling.cpp \
	ExponentialBackoff.cpp \
	ExtensibleRTTI.cpp \
	FileCollector.cpp \
	FileOutputBuffer.cpp \
	FileUtilities.cpp \
	FloatingPointMode.cpp \
	FoldingSet.cpp \
	FormatVariadic.cpp \
	FormattedStream.cpp \
	GlobPattern.cpp \
	GraphWriter.cpp \
	SupportHash.cpp \
	HexagonAttributeParser.cpp \
	HexagonAttributes.cpp \
	InitLLVM.cpp \
	InstructionCost.cpp \
	IntEqClasses.cpp \
	IntervalMap.cpp \
	Jobserver.cpp \
	JSON.cpp \
	KnownBits.cpp \
	KnownFPClass.cpp \
	LEB128.cpp \
	LineIterator.cpp \
	Locale.cpp \
	LockFileManager.cpp \
	MD5.cpp \
	MSP430AttributeParser.cpp \
	MSP430Attributes.cpp \
	ManagedStatic.cpp \
	MathExtras.cpp \
	MemAlloc.cpp \
	Memory.cpp \
	MemoryBuffer.cpp \
	MemoryBufferRef.cpp \
	ModRef.cpp \
	Mustache.cpp \
	NativeFormatting.cpp \
	OptimizedStructLayout.cpp \
	OptionStrCmp.cpp \
	Optional.cpp \
	PGOOptions.cpp \
	Parallel.cpp \
	Path.cpp \
	PluginLoader.cpp \
	PrettyStackTrace.cpp \
	Process.cpp \
	Program.cpp \
	ProgramStack.cpp \
	RISCVAttributeParser.cpp \
	RISCVAttributes.cpp \
	RISCVISAUtils.cpp \
	RWMutex.cpp \
	RandomNumberGenerator.cpp \
	IntegerInclusiveInterval.cpp \
	Regex.cpp \
	RewriteBuffer.cpp \
	RewriteRope.cpp \
	SHA1.cpp \
	SHA256.cpp \
	ScaledNumber.cpp \
	ScopedPrinter.cpp \
	Signals.cpp \
	Signposts.cpp \
	SipHash.cpp \
	SlowDynamicAPInt.cpp \
	SmallPtrSet.cpp \
	SmallVector.cpp \
	SourceMgr.cpp \
	SpecialCaseList.cpp \
	Statistic.cpp \
	StringExtras.cpp \
	StringMap.cpp \
	StringRef.cpp \
	StringSaver.cpp \
	SuffixTreeNode.cpp \
	SuffixTree.cpp \
	SystemUtils.cpp \
	TarWriter.cpp \
	TextEncoding.cpp \
	ThreadPool.cpp \
	Threading.cpp \
	TimeProfiler.cpp \
	Timer.cpp \
	ToolOutputFile.cpp \
	TrieRawHashMap.cpp \
	Twine.cpp \
	Unicode.cpp \
	UnicodeCaseFold.cpp \
	UnicodeNameToCodepoint.cpp \
	UnicodeNameToCodepointGenerated.cpp \
	Valgrind.cpp \
	VersionTuple.cpp \
	VirtualFileSystem.cpp \
	VirtualOutputBackend.cpp \
	VirtualOutputBackends.cpp \
	VirtualOutputConfig.cpp \
	VirtualOutputError.cpp \
	VirtualOutputFile.cpp \
	Watchdog.cpp \
	zOSLibFunctions.cpp \
	WithColor.cpp \
	YAMLParser.cpp \
	YAMLTraits.cpp \
	Z3Solver.cpp \
	circular_raw_ostream.cpp \
	raw_os_ostream.cpp \
	raw_ostream.cpp \
	raw_ostream_proxy.cpp \
	raw_socket_stream.cpp \
	regcomp.c \
	regerror.c \
	regexec.c \
	regfree.c \
	regstrlcpy.c \
	xxhash.cpp

# LSP
SRCS+=	Protocol.cpp \
	Transport.cpp \
	Logging.cpp

# BLAKE3
SRCS+=	blake3.c \
	blake3_dispatch.c \
	blake3_portable.c \
	blake3_neon.c

SupportAllocToken.cpp:
	ln -s ${LLVM_SRCS}/lib/Support/AllocToken.cpp $@

SupportAtomic.cpp:
	ln -s ${LLVM_SRCS}/lib/Support/Atomic.cpp $@

SupportError.cpp:
	ln -s ${LLVM_SRCS}/lib/Support/Error.cpp $@

SupportHash.cpp:
	ln -s ${LLVM_SRCS}/lib/Support/Hash.cpp $@

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Support
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Support/BLAKE3
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Support/LSP

install:
	@# Nothing here so far ...

CLEANFILES+=	SupportAllocToken.cpp \
		SupportAtomic.cpp \
		SupportError.cpp \
		SupportHash.cpp

.include <bsd.lib.mk>
