# $OpenBSD: Makefile,v 1.3 2017/10/04 20:57:09 patrick Exp $

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
