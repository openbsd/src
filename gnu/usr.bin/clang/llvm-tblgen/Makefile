# $OpenBSD: Makefile,v 1.15 2025/06/11 17:46:33 robert Exp $

PROG=	llvm-tblgen
NOMAN=

.include <bsd.own.mk>

# TableGen
SRCS=	ARMTargetDefEmitter.cpp \
	AsmMatcherEmitter.cpp \
	AsmWriterEmitter.cpp \
	Attributes.cpp \
	CTagsEmitter.cpp \
	CallingConvEmitter.cpp \
	CodeEmitterGen.cpp \
	CodeGenMapTable.cpp \
	CompressInstEmitter.cpp \
	DAGISelEmitter.cpp \
	DAGISelMatcherEmitter.cpp \
	DAGISelMatcherGen.cpp \
	DAGISelMatcherOpt.cpp \
	DFAEmitter.cpp \
	DFAPacketizerEmitter.cpp \
	DXILEmitter.cpp \
	DecoderEmitter.cpp \
	DirectiveEmitter.cpp \
	DisassemblerEmitter.cpp \
	ExegesisEmitter.cpp \
	FastISelEmitter.cpp \
	GlobalISelCombinerEmitter.cpp \
	GlobalISelEmitter.cpp \
	InstrDocsEmitter.cpp \
	InstrInfoEmitter.cpp \
	IntrinsicEmitter.cpp \
	MacroFusionPredicatorEmitter.cpp \
	OptParserEmitter.cpp \
	OptRSTEmitter.cpp \
	PseudoLoweringEmitter.cpp \
	RISCVTargetDefEmitter.cpp \
	RegisterBankEmitter.cpp \
	RegisterInfoEmitter.cpp \
	SearchableTableEmitter.cpp \
	SubtargetEmitter.cpp \
	TableGen.cpp \
	VTEmitter.cpp \
	WebAssemblyDisassemblerEmitter.cpp \
	X86DisassemblerTables.cpp \
	X86InstrMappingEmitter.cpp \
	X86FoldTablesEmitter.cpp \
	X86MnemonicTables.cpp \
	X86ModRMFilters.cpp \
	X86RecognizableInstr.cpp

# TableGen/Basic
SRCS+=	CodeGenIntrinsics.cpp \
	SDNodeProperties.cpp

# TableGen/Common
SRCS+=	AsmWriterInst.cpp \
	CodeGenDAGPatterns.cpp \
	CodeGenHwModes.cpp \
	CodeGenInstAlias.cpp \
	CodeGenInstruction.cpp \
	CodeGenRegisters.cpp \
	CodeGenSchedule.cpp \
	CodeGenTarget.cpp \
	DAGISelMatcher.cpp \
	InfoByHwMode.cpp \
	OptEmitter.cpp \
	PredicateExpander.cpp \
	SubtargetFeatureInfo.cpp \
	Types.cpp \
	VarLenCodeEmitterGen.cpp

# TableGen/Common/GlobalISel
SRCS+=	CodeExpander.cpp \
	CombinerUtils.cpp \
	CXXPredicates.cpp \
	GlobalISelMatchTable.cpp \
	GlobalISelMatchTableExecutorEmitter.cpp \
	PatternParser.cpp \
	Patterns.cpp

CPPFLAGS+=	-I${.CURDIR}/../../../llvm/llvm/utils/TableGen

.PATH:	${.CURDIR}/../../../llvm/llvm/utils/TableGen \
	${.CURDIR}/../../../llvm/llvm/utils/TableGen/Basic \
	${.CURDIR}/../../../llvm/llvm/utils/TableGen/Common \
	${.CURDIR}/../../../llvm/llvm/utils/TableGen/Common/GlobalISel

LLVM_LIBDEPS=	LLVMTableGen LLVMSupport

CLEANFILES+=	llvm/CodeGen/GenVT.inc CodeGen llvm

all: GenVT.inc

GenVT.inc: ${.CURDIR}/../../../llvm/llvm/include/llvm/CodeGen/ValueTypes.td
	ln -sf ${.OBJDIR} llvm
	ln -sf ${.OBJDIR} llvm/CodeGen
	${.OBJDIR}/../llvm-min-tblgen/llvm-min-tblgen -gen-vt \
                -o llvm/CodeGen/${.TARGET} ${.ALLSRC}

install:
	${INSTALL} ${INSTALL_COPY} -m 444 llvm/CodeGen/GenVT.inc \
		${DESTDIR}/usr/include/llvm/CodeGen/GenVT.inc

.include <bsd.prog.mk>
