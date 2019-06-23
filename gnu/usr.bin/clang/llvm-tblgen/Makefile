# $OpenBSD: Makefile,v 1.10 2019/06/23 22:07:45 patrick Exp $

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
	DFAPacketizerEmitter.cpp \
	DisassemblerEmitter.cpp \
	ExegesisEmitter.cpp \
	FastISelEmitter.cpp \
	FixedLenDecoderEmitter.cpp \
	GlobalISelEmitter.cpp \
	InfoByHwMode.cpp \
	InstrDocsEmitter.cpp \
	InstrInfoEmitter.cpp \
	IntrinsicEmitter.cpp \
	OptParserEmitter.cpp \
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

.PATH:	${.CURDIR}/../../../llvm/utils/TableGen

LLVM_LIBDEPS=	LLVMTableGen LLVMSupport

install:
	@# Nothing here so far ...

.include <bsd.prog.mk>
