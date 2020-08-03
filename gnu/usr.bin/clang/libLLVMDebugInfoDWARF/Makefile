# $OpenBSD: Makefile,v 1.7 2020/08/03 14:45:25 patrick Exp $

LIB=	LLVMDebugInfoDWARF
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/DebugInfo/DWARF

SRCS+=	DWARFAbbreviationDeclaration.cpp \
	DWARFAcceleratorTable.cpp \
	DWARFAddressRange.cpp \
	DWARFCompileUnit.cpp \
	DWARFContext.cpp \
	DWARFDataExtractor.cpp \
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
	DWARFExpression.cpp \
	DWARFFormValue.cpp \
	DWARFGdbIndex.cpp \
	DWARFListTable.cpp \
	DWARFTypeUnit.cpp \
	DWARFUnitIndex.cpp \
	DWARFUnit.cpp \
	DWARFVerifier.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/DebugInfo/DWARF
