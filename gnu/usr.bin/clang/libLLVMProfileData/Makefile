# $OpenBSD: Makefile,v 1.8 2025/06/11 17:46:32 robert Exp $

LIB=	LLVMProfileData
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/ProfileData

SRCS+=	GCOV.cpp \
	InstrProf.cpp \
	InstrProfCorrelator.cpp \
	InstrProfReader.cpp \
	InstrProfWriter.cpp \
	ItaniumManglingCanonicalizer.cpp \
	MemProf.cpp \
	MemProfReader.cpp \
	PGOCtxProfReader.cpp \
	PGOCtxProfWriter.cpp \
	ProfileSummaryBuilder.cpp \
	SampleProf.cpp \
	SampleProfReader.cpp \
	SampleProfWriter.cpp \
	SymbolRemappingReader.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ProfileData
