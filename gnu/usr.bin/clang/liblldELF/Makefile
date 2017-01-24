# $OpenBSD: Makefile,v 1.2 2017/01/24 08:44:47 patrick Exp $

LIB=	lldELF
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/tools/lld/include

.include <bsd.own.mk>
SRCS=	Driver.cpp \
	DriverUtils.cpp \
	EhFrame.cpp \
	Error.cpp \
	GdbIndex.cpp \
	ICF.cpp \
	InputFiles.cpp \
	InputSection.cpp \
	LinkerScript.cpp \
	LTO.cpp \
	MarkLive.cpp \
	Mips.cpp \
	OutputSections.cpp \
	Relocations.cpp \
	ScriptParser.cpp \
	Strings.cpp \
	SymbolTable.cpp \
	Symbols.cpp \
	SyntheticSections.cpp \
	Target.cpp \
	Thunks.cpp \
	Writer.cpp

.PATH:	${.CURDIR}/../../../llvm/tools/lld/ELF

Driver.o: Options.inc

Options.inc: Options.td
	${.OBJDIR}/../llvm-tblgen/llvm-tblgen -gen-opt-parser-defs \
		-I ${LLVM_SRCS}/include \
		-o ${.TARGET} ${.ALLSRC}

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
