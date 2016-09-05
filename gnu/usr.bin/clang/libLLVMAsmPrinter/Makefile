# $OpenBSD: Makefile,v 1.1 2016/09/05 10:56:49 pascal Exp $

LIB=	LLVMAsmPrinter
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	AddressPool.cpp \
	ARMException.cpp \
	AsmPrinter.cpp \
	AsmPrinterDwarf.cpp \
	AsmPrinterInlineAsm.cpp \
	DbgValueHistoryCalculator.cpp \
	DebugLocStream.cpp \
	DIE.cpp \
	DIEHash.cpp \
	DwarfAccelTable.cpp \
	DwarfCFIException.cpp \
	DwarfCompileUnit.cpp \
	DwarfDebug.cpp \
	DwarfExpression.cpp \
	DwarfFile.cpp \
	DwarfStringPool.cpp \
	DwarfUnit.cpp \
	EHStreamer.cpp \
	ErlangGCPrinter.cpp \
	OcamlGCPrinter.cpp \
	WinException.cpp \
	WinCodeViewLineTables.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/CodeGen/AsmPrinter

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
