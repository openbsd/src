# $OpenBSD: Makefile,v 1.4 2019/03/05 09:44:41 jsg Exp $

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

.PATH:	${.CURDIR}/../../../llvm/lib/ProfileData
