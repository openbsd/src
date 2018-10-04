# $OpenBSD: Makefile,v 1.1 2018/10/04 11:43:29 patrick Exp $

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
	FormattersHelpers.cpp \
	LanguageCategory.cpp \
	StringPrinter.cpp \
	TypeCategory.cpp \
	TypeCategoryMap.cpp \
	TypeFormat.cpp \
	TypeSummary.cpp \
	TypeSynthetic.cpp \
	TypeValidator.cpp \
	ValueObjectPrinter.cpp \
	VectorType.cpp \

.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/DataFormatters

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
