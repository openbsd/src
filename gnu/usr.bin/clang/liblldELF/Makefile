# $OpenBSD: Makefile,v 1.7 2019/06/23 22:07:42 patrick Exp $

LIB=	lldELF
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/tools/lld/include
CPPFLAGS+=	-I${LLVM_SRCS}/tools/lld/ELF

.include <bsd.own.mk>
SRCS=	AArch64.cpp \
	AArch64ErrataFix.cpp \
	AMDGPU.cpp \
	ARM.cpp \
	AVR.cpp \
	CallGraphSort.cpp \
	DWARF.cpp \
	Driver.cpp \
	DriverUtils.cpp \
	EhFrame.cpp \
	Filesystem.cpp \
	Hexagon.cpp \
	ICF.cpp \
	InputFiles.cpp \
	InputSection.cpp \
	LinkerScript.cpp \
	LTO.cpp \
	MSP430.cpp \
	MapFile.cpp \
	MarkLive.cpp \
	Mips.cpp \
	MipsArchTree.cpp \
	OutputSections.cpp \
	PPC.cpp \
	PPC64.cpp \
	RISCV.cpp \
	Relocations.cpp \
	SPARCV9.cpp \
	ScriptLexer.cpp \
	ScriptParser.cpp \
	SymbolTable.cpp \
	Symbols.cpp \
	SyntheticSections.cpp \
	Target.cpp \
	Thunks.cpp \
	Writer.cpp \
	X86.cpp \
	X86_64.cpp

.PATH:	${.CURDIR}/../../../llvm/tools/lld/ELF
.PATH:	${.CURDIR}/../../../llvm/tools/lld/ELF/Arch

Driver.o: Options.inc

Options.inc: Options.td
	${.OBJDIR}/../llvm-tblgen/llvm-tblgen -gen-opt-parser-defs \
		-I ${LLVM_SRCS}/include \
		-o ${.TARGET} ${.ALLSRC}

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
