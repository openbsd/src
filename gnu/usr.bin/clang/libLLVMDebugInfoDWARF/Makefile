# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:35 espie Exp $

LIB=	LLVMDebugInfoDWARF
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/DebugInfo/DWARF

.include <bsd.own.mk>
SRCS=	DWARFAbbreviationDeclaration.cpp \
	DWARFAcceleratorTable.cpp \
	DWARFCompileUnit.cpp \
	DWARFContext.cpp \
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
	SyntaxHighlighting.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/DebugInfo/DWARF

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
