# $OpenBSD: Makefile,v 1.17 2026/05/29 11:18:32 robert Exp $

PROG=	llvm-tblgen
NOMAN=

.include <bsd.own.mk>

# TableGen
SRCS=	AsmMatcherEmitter.cpp \
	AsmWriterEmitter.cpp \
	CTagsEmitter.cpp \
	CallingConvEmitter.cpp \
	CodeEmitterGen.cpp \
	CodeGenMapTable.cpp \
	CompressInstEmitter.cpp \
	DAGISelEmitter.cpp \
	DAGISelMatcher.cpp \
	DAGISelMatcherEmitter.cpp \
	DAGISelMatcherGen.cpp \
	DAGISelMatcherOpt.cpp \
	DFAEmitter.cpp \
	DFAPacketizerEmitter.cpp \
	DXILEmitter.cpp \
	DecoderEmitter.cpp \
	DecoderTableEmitter.cpp \
	DecoderTree.cpp \
	DisassemblerEmitter.cpp \
	ExegesisEmitter.cpp \
	FastISelEmitter.cpp \
	GlobalISelCombinerEmitter.cpp \
	GlobalISelEmitter.cpp \
	InstrDocsEmitter.cpp \
	InstrInfoEmitter.cpp \
	MacroFusionPredicatorEmitter.cpp \
	OptionParserEmitter.cpp \
	OptionRSTEmitter.cpp \
	PseudoLoweringEmitter.cpp \
	RegisterBankEmitter.cpp \
	RegisterInfoEmitter.cpp \
	SDNodeInfoEmitter.cpp \
	SearchableTableEmitter.cpp \
	SubtargetEmitter.cpp \
	WebAssemblyDisassemblerEmitter.cpp \
	X86DisassemblerTables.cpp \
	X86InstrMappingEmitter.cpp \
	X86FoldTablesEmitter.cpp \
	X86MnemonicTables.cpp \
	X86ModRMFilters.cpp \
	X86RecognizableInstr.cpp \
	llvm-tblgen.cpp

# TableGen/Basic
SRCS+=	ARMTargetDefEmitter.cpp \
	Attributes.cpp \
	CodeGenIntrinsics.cpp \
	DirectiveEmitter.cpp \
	IntrinsicEmitter.cpp \
	RISCVTargetDefEmitter.cpp \
	RuntimeLibcallsEmitter.cpp \
	RuntimeLibcalls.cpp \
	SDNodeProperties.cpp \
	TableGen.cpp \
	TargetFeaturesEmitter.cpp \
	TargetLibraryInfoEmitter.cpp \
	VTEmitter.cpp

# TableGen/Common
SRCS+=	AsmWriterInst.cpp \
	CodeGenDAGPatterns.cpp \
	CodeGenHwModes.cpp \
	CodeGenInstAlias.cpp \
	CodeGenInstruction.cpp \
	CodeGenRegisters.cpp \
	CodeGenSchedule.cpp \
	CodeGenTarget.cpp \
	InfoByHwMode.cpp \
	InstructionEncoding.cpp \
	OptEmitter.cpp \
	PredicateExpander.cpp \
	SubtargetFeatureInfo.cpp \
	Types.cpp \
	Utils.cpp \
	VarLenCodeEmitterGen.cpp

# TableGen/Common/GlobalISel
SRCS+=	CodeExpander.cpp \
	CombinerUtils.cpp \
	CXXPredicates.cpp \
	GlobalISelMatchTable.cpp \
	GlobalISelMatchTableExecutorEmitter.cpp \
	PatternParser.cpp \
	Patterns.cpp

CXXFLAGS+=	-D_LIBCPP_USE_LOWER_DEQUE_BLOCK_SIZE
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
