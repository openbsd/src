# $OpenBSD: Makefile,v 1.4 2018/04/06 14:44:04 patrick Exp $

LIB=	LLVMDebugInfoDWARF
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/DebugInfo/DWARF

.include <bsd.own.mk>
SRCS=	DWARFAbbreviationDeclaration.cpp \
	DWARFAcceleratorTable.cpp \
	DWARFCompileUnit.cpp \
	DWARFContext.cpp \
	DWARFDataExtractor.cpp \
	DWARFDebugAbbrev.cpp \
	DWARFDebugArangeSet.cpp \
	DWARFDebugAranges.cpp \
	DWARFDebugFrame.cpp \
	DWARFDebugInfoEntry.cpp \
	DWARFDebugLine.cpp \
	DWARFDebugLoc.cpp \
	DWARFDebugMacro.cpp \
	DWARFDebugPubTable.cpp \
	DWARFDebugRangeList.cpp \
	DWARFDie.cpp \
	DWARFExpression.cpp \
	DWARFFormValue.cpp \
	DWARFGdbIndex.cpp \
	DWARFTypeUnit.cpp \
	DWARFUnitIndex.cpp \
	DWARFUnit.cpp \
	DWARFVerifier.cpp \
	SyntaxHighlighting.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/DebugInfo/DWARF

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
