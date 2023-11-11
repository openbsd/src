# $OpenBSD: Makefile,v 1.7 2023/11/11 18:35:37 robert Exp $

LIB=	LLVMProfileData
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/ProfileData

SRCS+=	GCOV.cpp \
	InstrProf.cpp \
	InstrProfCorrelator.cpp \
	InstrProfReader.cpp \
	InstrProfWriter.cpp \
	MemProf.cpp \
	ProfileSummaryBuilder.cpp \
	SampleProf.cpp \
	SampleProfReader.cpp \
	SampleProfWriter.cpp \
	RawMemProfReader.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ProfileData
