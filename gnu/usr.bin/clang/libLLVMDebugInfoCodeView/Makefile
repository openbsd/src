# $OpenBSD: Makefile,v 1.5 2018/04/06 14:44:04 patrick Exp $

LIB=	LLVMDebugInfoCodeView
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/DebugInfo/CodeView

.include <bsd.own.mk>
SRCS=	CVTypeVisitor.cpp \
	CodeViewError.cpp \
	CodeViewRecordIO.cpp \
	ContinuationRecordBuilder.cpp \
	Formatters.cpp \
	GlobalTypeTableBuilder.cpp \
	Line.cpp \
	RecordName.cpp \
	RecordSerialization.cpp \
	SimpleTypeSerializer.cpp \
	SymbolDumper.cpp \
	SymbolRecordMapping.cpp \
	TypeDumpVisitor.cpp \
	TypeHashing.cpp \
	TypeIndex.cpp \
	TypeIndexDiscovery.cpp \
	TypeRecordMapping.cpp \
	TypeStreamMerger.cpp \
	TypeTableCollection.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/DebugInfo/CodeView

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
