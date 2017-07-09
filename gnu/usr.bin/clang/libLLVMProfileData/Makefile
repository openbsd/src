# $OpenBSD: Makefile,v 1.3 2017/07/09 15:28:35 espie Exp $

LIB=	LLVMProfileData
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/ProfileData

.include <bsd.own.mk>
SRCS=	InstrProf.cpp \
	InstrProfReader.cpp \
	InstrProfWriter.cpp \
	ProfileSummaryBuilder.cpp \
	SampleProf.cpp \
	SampleProfReader.cpp \
	SampleProfWriter.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/ProfileData

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
