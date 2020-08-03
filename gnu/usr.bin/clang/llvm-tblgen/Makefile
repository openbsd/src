# $OpenBSD: Makefile,v 1.11 2020/08/03 14:45:31 patrick Exp $

PROG=	llvm-tblgen
NOMAN=

.include <bsd.own.mk>
SRCS=	AsmMatcherEmitter.cpp \
	AsmWriterEmitter.cpp \
	AsmWriterInst.cpp \
	Attributes.cpp \
	CTagsEmitter.cpp \
	CallingConvEmitter.cpp \
	CodeEmitterGen.cpp \
	CodeExpander.cpp \
	CodeGenDAGPatterns.cpp \
	CodeGenHwModes.cpp \
	CodeGenInstruction.cpp \
	CodeGenMapTable.cpp \
	CodeGenRegisters.cpp \
	CodeGenSchedule.cpp \
	CodeGenTarget.cpp \
	DAGISelEmitter.cpp \
	DAGISelMatcherEmitter.cpp \
	DAGISelMatcherGen.cpp \
	DAGISelMatcherOpt.cpp \
	DAGISelMatcher.cpp \
	DFAEmitter.cpp \
	DFAPacketizerEmitter.cpp \
	DisassemblerEmitter.cpp \
	ExegesisEmitter.cpp \
	FastISelEmitter.cpp \
	FixedLenDecoderEmitter.cpp \
	GICombinerEmitter.cpp \
	GIMatchDag.cpp \
	GIMatchDagEdge.cpp \
	GIMatchDagInstr.cpp \
	GIMatchDagOperands.cpp \
	GIMatchDagPredicate.cpp \
	GIMatchDagPredicateDependencyEdge.cpp \
	GIMatchTree.cpp \
	GlobalISelEmitter.cpp \
	InfoByHwMode.cpp \
	InstrDocsEmitter.cpp \
	InstrInfoEmitter.cpp \
	IntrinsicEmitter.cpp \
	OptEmitter.cpp \
	OptParserEmitter.cpp \
	OptRSTEmitter.cpp \
	PredicateExpander.cpp \
	PseudoLoweringEmitter.cpp \
	RISCVCompressInstEmitter.cpp \
	RegisterBankEmitter.cpp \
	RegisterInfoEmitter.cpp \
	SDNodeProperties.cpp \
	SearchableTableEmitter.cpp \
	SubtargetEmitter.cpp \
	SubtargetFeatureInfo.cpp \
	TableGen.cpp \
	Types.cpp \
	WebAssemblyDisassemblerEmitter.cpp \
	X86DisassemblerTables.cpp \
	X86EVEX2VEXTablesEmitter.cpp \
	X86FoldTablesEmitter.cpp \
	X86ModRMFilters.cpp \
	X86RecognizableInstr.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/utils/TableGen
.PATH:	${.CURDIR}/../../../llvm/llvm/utils/TableGen/GlobalISel

LLVM_LIBDEPS=	LLVMTableGen LLVMSupport

install:
	@# Nothing here so far ...

.include <bsd.prog.mk>
