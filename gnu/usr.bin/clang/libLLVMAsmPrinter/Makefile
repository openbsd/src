# $OpenBSD: Makefile,v 1.4 2019/01/27 16:59:41 patrick Exp $

LIB=	LLVMAsmPrinter
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	ARMException.cpp \
	AccelTable.cpp \
	AddressPool.cpp \
	AsmPrinter.cpp \
	AsmPrinterDwarf.cpp \
	AsmPrinterInlineAsm.cpp \
	CodeViewDebug.cpp \
	DbgValueHistoryCalculator.cpp \
	DebugHandlerBase.cpp \
	DebugLocStream.cpp \
	DIE.cpp \
	DIEHash.cpp \
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
	WinCFGuard.cpp \
	WinException.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/CodeGen/AsmPrinter

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
