# $OpenBSD: Makefile,v 1.2 2017/01/24 08:44:47 patrick Exp $

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

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
