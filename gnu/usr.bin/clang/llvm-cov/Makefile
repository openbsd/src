# $OpenBSD: Makefile,v 1.1 2022/08/08 09:25:13 fcambus Exp $

.include <bsd.own.mk>

PROG=	llvm-cov
BINDIR=	/usr/bin

SRCS=	llvm-cov.cpp \
	gcov.cpp \
	CodeCoverage.cpp \
	CoverageExporterJson.cpp \
	CoverageExporterLcov.cpp \
	CoverageFilters.cpp \
	CoverageReport.cpp \
	CoverageSummaryInfo.cpp \
	SourceCoverageView.cpp \
	SourceCoverageViewHTML.cpp \
	SourceCoverageViewText.cpp \
	TestingSupport.cpp \

MAN+=	llvm-cov.1

.PATH:	${.CURDIR}/../../../llvm/llvm/tools/llvm-cov

LLVM_LIBDEPS=	LLVM

LDADD+= -L ${.OBJDIR}/../libLLVM -lLLVM

.include <bsd.prog.mk>
