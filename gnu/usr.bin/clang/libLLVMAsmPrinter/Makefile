# $OpenBSD: Makefile,v 1.3 2017/07/09 15:28:34 espie Exp $

LIB=	LLVMAsmPrinter
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	AddressPool.cpp \
	ARMException.cpp \
	AsmPrinter.cpp \
	AsmPrinterDwarf.cpp \
	AsmPrinterInlineAsm.cpp \
	CodeViewDebug.cpp \
	DbgValueHistoryCalculator.cpp \
	DebugHandlerBase.cpp \
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
	WinException.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/CodeGen/AsmPrinter

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
