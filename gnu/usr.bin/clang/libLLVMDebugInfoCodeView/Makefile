# $OpenBSD: Makefile,v 1.3 2017/07/09 15:28:35 espie Exp $

LIB=	LLVMDebugInfoCodeView
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/DebugInfo/CodeView

.include <bsd.own.mk>
SRCS=	CVTypeDumper.cpp \
	CVTypeVisitor.cpp \
	CodeViewError.cpp \
	CodeViewRecordIO.cpp \
	Line.cpp \
	ModuleSubstream.cpp \
	ModuleSubstreamVisitor.cpp \
	RecordSerialization.cpp \
	SymbolDumper.cpp \
	TypeDatabase.cpp \
	TypeDatabaseVisitor.cpp \
	TypeDumpVisitor.cpp \
	TypeRecord.cpp \
	TypeRecordMapping.cpp \
	TypeSerializer.cpp \
	TypeStreamMerger.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/DebugInfo/CodeView

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
