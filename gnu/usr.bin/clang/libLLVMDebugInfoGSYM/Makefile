# $OpenBSD: Makefile,v 1.1 2026/05/29 11:18:30 robert Exp $

LIB=	LLVMDebugInfoGSYM
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/DebugInfo/GSYM

SRCS+=	DwarfTransformer.cpp \
	Header.cpp \
	FileWriter.cpp \
	FunctionInfo.cpp \
	GsymCreator.cpp \
	GsymContext.cpp \
	GsymReader.cpp \
	InlineInfo.cpp \
	LineTable.cpp \
	LookupResult.cpp \
	CallSiteInfo.cpp \
	MergedFunctionsInfo.cpp \
	ObjectFileTransformer.cpp \
	ExtractRanges.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/DebugInfo/GSYM
