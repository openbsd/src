# $OpenBSD: Makefile,v 1.4 2020/08/03 14:45:25 patrick Exp $

LIB=	LLVMCoverage
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/ProfileData/Coverage

SRCS+=	CoverageMapping.cpp \
	CoverageMappingReader.cpp \
	CoverageMappingWriter.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ProfileData/Coverage
