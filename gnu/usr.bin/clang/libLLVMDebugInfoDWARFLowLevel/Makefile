# $OpenBSD: Makefile,v 1.1 2026/05/29 11:18:30 robert Exp $

LIB=	LLVMDebugInfoDWARFLowLevel
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/DebugInfo/DWARF \
	-I${LLVM_SRCS}/include/llvm/DebugInfo

SRCS+=	DWARFCFIProgram.cpp \
	DWARFExpression.cpp \
	DWARFUnwindTable.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/DebugInfo/DWARF/LowLevel
