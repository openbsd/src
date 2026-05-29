# $OpenBSD: Makefile,v 1.4 2026/05/29 11:18:31 robert Exp $

LIB=	lldbDataFormatters
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	CXXFunctionPointer.cpp \
	DataVisualization.cpp \
	DumpValueObjectOptions.cpp \
	FormatCache.cpp \
	FormatClasses.cpp \
	FormatManager.cpp \
	FormatterBytecode.cpp \
	FormattersHelpers.cpp \
	FormatterSection.cpp \
	LanguageCategory.cpp \
	StringPrinter.cpp \
	TypeCategory.cpp \
	TypeCategoryMap.cpp \
	TypeFormat.cpp \
	TypeSummary.cpp \
	TypeSynthetic.cpp \
	ValueObjectPrinter.cpp \
	VectorType.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/DataFormatters

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
