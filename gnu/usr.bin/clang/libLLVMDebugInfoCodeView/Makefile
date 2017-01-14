# $OpenBSD: Makefile,v 1.1 2017/01/14 20:12:41 patrick Exp $

LIB=	LLVMDebugInfoCodeView
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/DebugInfo/CodeView

.include <bsd.own.mk>
SRCS=	ByteStream.cpp \
	CVTypeVisitor.cpp \
	CodeViewError.cpp \
	FieldListRecordBuilder.cpp \
	Line.cpp \
	ListRecordBuilder.cpp \
	MemoryTypeTableBuilder.cpp \
	ModuleSubstream.cpp \
	ModuleSubstreamVisitor.cpp \
	RecordSerialization.cpp \
	StreamReader.cpp \
	StreamWriter.cpp \
	SymbolDumper.cpp \
	TypeDumper.cpp \
	TypeRecord.cpp \
	TypeRecordBuilder.cpp \
	TypeStreamMerger.cpp \
	TypeTableBuilder.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/DebugInfo/CodeView

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
