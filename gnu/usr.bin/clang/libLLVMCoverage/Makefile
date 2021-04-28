# $OpenBSD: Makefile,v 1.5 2021/04/28 12:55:39 patrick Exp $

LIB=	LLVMCoverage
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/ProfileData/Coverage

SRCS+=	CoverageMapping.cpp \
	CoverageMappingWriter.cpp \
	CoverageMappingReader.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ProfileData/Coverage
