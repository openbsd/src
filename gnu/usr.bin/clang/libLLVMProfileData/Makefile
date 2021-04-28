# $OpenBSD: Makefile,v 1.6 2021/04/28 12:55:40 patrick Exp $

LIB=	LLVMProfileData
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/ProfileData

SRCS+=	GCOV.cpp \
	InstrProf.cpp \
	InstrProfReader.cpp \
	InstrProfWriter.cpp \
	ProfileSummaryBuilder.cpp \
	SampleProf.cpp \
	SampleProfReader.cpp \
	SampleProfWriter.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ProfileData
