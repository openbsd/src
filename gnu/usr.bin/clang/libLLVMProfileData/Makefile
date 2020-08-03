# $OpenBSD: Makefile,v 1.5 2020/08/03 14:45:27 patrick Exp $

LIB=	LLVMProfileData
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/ProfileData

SRCS+=	InstrProf.cpp \
	InstrProfReader.cpp \
	InstrProfWriter.cpp \
	ProfileSummaryBuilder.cpp \
	SampleProf.cpp \
	SampleProfReader.cpp \
	SampleProfWriter.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ProfileData
