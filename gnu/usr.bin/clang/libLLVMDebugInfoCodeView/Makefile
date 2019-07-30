# $OpenBSD: Makefile,v 1.4 2017/10/04 20:57:09 patrick Exp $

LIB=	LLVMDebugInfoCodeView
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/DebugInfo/CodeView

.include <bsd.own.mk>
SRCS=	CVTypeVisitor.cpp \
	CodeViewError.cpp \
	CodeViewRecordIO.cpp \
	Formatters.cpp \
	Line.cpp \
	RecordSerialization.cpp \
	SymbolDumper.cpp \
	TypeDumpVisitor.cpp \
	TypeIndex.cpp \
	TypeName.cpp \
	TypeRecordMapping.cpp \
	TypeSerializer.cpp \
	TypeStreamMerger.cpp \
	TypeTableCollection.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/DebugInfo/CodeView

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
