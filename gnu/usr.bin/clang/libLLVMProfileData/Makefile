# $OpenBSD: Makefile,v 1.9 2026/05/29 11:18:31 robert Exp $

LIB=	LLVMProfileData
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/ProfileData

SRCS+=	DataAccessProf.cpp \
	GCOV.cpp \
	IndexedMemProfData.cpp \
	InstrProf.cpp \
	InstrProfCorrelator.cpp \
	InstrProfReader.cpp \
	InstrProfWriter.cpp \
	ItaniumManglingCanonicalizer.cpp \
	MemProf.cpp \
	MemProfCommon.cpp \
	MemProfReader.cpp \
	MemProfRadixTree.cpp \
	MemProfSummary.cpp \
	MemProfSummaryBuilder.cpp \
	PGOCtxProfReader.cpp \
	PGOCtxProfWriter.cpp \
	ProfileSummaryBuilder.cpp \
	SampleProf.cpp \
	SampleProfReader.cpp \
	SampleProfWriter.cpp \
	SymbolRemappingReader.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ProfileData
