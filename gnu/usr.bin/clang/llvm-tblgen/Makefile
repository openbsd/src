# $OpenBSD: Makefile,v 1.1 2016/09/05 10:56:50 pascal Exp $

PROG=	llvm-tblgen
NOMAN=

LDADD+=	-ltermlib
DPADD+=	${LIBTERMLIB}

SRCS=	AsmMatcherEmitter.cpp \
	AsmWriterEmitter.cpp \
	AsmWriterInst.cpp \
	Attributes.cpp \
	CallingConvEmitter.cpp \
	CodeEmitterGen.cpp \
	CodeGenDAGPatterns.cpp \
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
	FastISelEmitter.cpp \
	FixedLenDecoderEmitter.cpp \
	InstrInfoEmitter.cpp \
	IntrinsicEmitter.cpp \
	OptParserEmitter.cpp \
	PseudoLoweringEmitter.cpp \
	RegisterInfoEmitter.cpp \
	SubtargetEmitter.cpp \
	TableGen.cpp \
	X86DisassemblerTables.cpp \
	X86ModRMFilters.cpp \
	X86RecognizableInstr.cpp \
	CTagsEmitter.cpp

.PATH:	${.CURDIR}/../../../llvm/utils/TableGen

LLVM_LIBDEPS=	LLVMTableGen LLVMSupport

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.prog.mk>
