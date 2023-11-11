# $OpenBSD: Makefile,v 1.10 2023/11/11 18:35:39 robert Exp $

LIB=	lldELF
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/lld/ELF
CPPFLAGS+=	-I${LLD_SRCS}/include
CPPFLAGS+=	-I${LLD_SRCS}/ELF

.include <bsd.own.mk>

# ELF/Arch
SRCS=	AArch64.cpp \
	AMDGPU.cpp \
	ARM.cpp \
	AVR.cpp \
	Hexagon.cpp \
	Mips.cpp \
	MipsArchTree.cpp \
	MSP430.cpp \
	PPC.cpp \
	PPC64.cpp \
	RISCV.cpp \
	SPARCV9.cpp \
	X86.cpp \
	X86_64.cpp

# ELF
SRCS+=	AArch64ErrataFix.cpp \
	ARMErrataFix.cpp \
	CallGraphSort.cpp \
	DWARF.cpp \
	Driver.cpp \
	DriverUtils.cpp \
	EhFrame.cpp \
	ICF.cpp \
	InputFiles.cpp \
	InputSection.cpp \
	LTO.cpp \
	LinkerScript.cpp \
	MapFile.cpp \
	MarkLive.cpp \
	OutputSections.cpp \
	Relocations.cpp \
	ScriptLexer.cpp \
	ScriptParser.cpp \
	SymbolTable.cpp \
	Symbols.cpp \
	SyntheticSections.cpp \
	Target.cpp \
	Thunks.cpp \
	Writer.cpp

.PATH:	${.CURDIR}/../../../llvm/lld/ELF
.PATH:	${.CURDIR}/../../../llvm/lld/ELF/Arch

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
