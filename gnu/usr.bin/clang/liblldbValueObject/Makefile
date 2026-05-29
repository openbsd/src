# $OpenBSD: Makefile,v 1.1 2026/05/29 11:18:32 robert Exp $

LIB=	lldbValueObject
NOPIC=
NOPROFILE=

.include <bsd.own.mk>

SRCS=	DILAST.cpp \
	DILEval.cpp \
	DILLexer.cpp \
	DILParser.cpp \
	ValueObject.cpp \
	ValueObjectCast.cpp \
	ValueObjectChild.cpp \
	ValueObjectConstResult.cpp \
	ValueObjectConstResultCast.cpp \
	ValueObjectConstResultChild.cpp \
	ValueObjectConstResultImpl.cpp \
	ValueObjectDynamicValue.cpp \
	ValueObjectList.cpp \
	ValueObjectMemory.cpp \
	ValueObjectRegister.cpp \
	ValueObjectSynthetic.cpp \
	ValueObjectUpdater.cpp \
	ValueObjectVariable.cpp \
	ValueObjectVTable.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/ValueObject

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
