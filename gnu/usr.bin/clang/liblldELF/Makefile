# $OpenBSD: Makefile,v 1.8 2020/08/03 14:45:29 patrick Exp $

LIB=	lldELF
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/lld/ELF
CPPFLAGS+=	-I${LLD_SRCS}/include
CPPFLAGS+=	-I${LLD_SRCS}/ELF

.include <bsd.own.mk>
SRCS=	AArch64.cpp \
	AArch64ErrataFix.cpp \
	AMDGPU.cpp \
	ARM.cpp \
	ARMErrataFix.cpp \
	AVR.cpp \
	CallGraphSort.cpp \
	DWARF.cpp \
	Driver.cpp \
	DriverUtils.cpp \
	EhFrame.cpp \
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

.PATH:	${.CURDIR}/../../../llvm/lld/ELF
.PATH:	${.CURDIR}/../../../llvm/lld/ELF/Arch

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
