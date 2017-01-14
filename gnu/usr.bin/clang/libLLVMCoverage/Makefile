# $OpenBSD: Makefile,v 1.1 2017/01/14 20:12:41 patrick Exp $

LIB=	LLVMCoverage
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/ProfileData/Coverage

.include <bsd.own.mk>
SRCS=	CoverageMapping.cpp \
	CoverageMappingReader.cpp \
	CoverageMappingWriter.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/ProfileData/Coverage

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
