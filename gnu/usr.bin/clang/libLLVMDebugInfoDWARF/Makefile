# $OpenBSD: Makefile,v 1.10 2026/05/29 11:18:30 robert Exp $

LIB=	LLVMDebugInfoDWARF
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/DebugInfo/DWARF \
	-I${LLVM_SRCS}/include/llvm/DebugInfo

SRCS+=	DWARFAbbreviationDeclaration.cpp \
	DWARFAddressRange.cpp \
	DWARFAcceleratorTable.cpp \
	DWARFCFIPrinter.cpp \
	DWARFCompileUnit.cpp \
	DWARFContext.cpp \
	DWARFDebugAbbrev.cpp \
	DWARFDebugAddr.cpp \
	DWARFDebugArangeSet.cpp \
	DWARFDebugAranges.cpp \
	DWARFDebugFrame.cpp \
	DWARFDebugInfoEntry.cpp \
	DWARFDebugLine.cpp \
	DWARFDebugLoc.cpp \
	DWARFDebugMacro.cpp \
	DWARFDebugPubTable.cpp \
	DWARFDebugRangeList.cpp \
	DWARFDebugRnglists.cpp \
	DWARFDie.cpp \
	DWARFExpressionPrinter.cpp \
	DWARFFormValue.cpp \
	DWARFGdbIndex.cpp \
	DWARFListTable.cpp \
	DWARFLocationExpression.cpp \
	DWARFTypeUnit.cpp \
	DWARFUnwindTablePrinter.cpp \
	DWARFUnitIndex.cpp \
	DWARFUnit.cpp \
	DWARFVerifier.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/DebugInfo/DWARF
