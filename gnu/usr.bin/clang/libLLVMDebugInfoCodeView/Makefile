# $OpenBSD: Makefile,v 1.6 2018/10/04 11:43:29 patrick Exp $

LIB=	LLVMDebugInfoCodeView
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/DebugInfo/CodeView

.include <bsd.own.mk>
SRCS=	AppendingTypeTableBuilder.cpp \
	CodeViewError.cpp \
	CodeViewRecordIO.cpp \
	ContinuationRecordBuilder.cpp \
	CVSymbolVisitor.cpp \
	CVTypeVisitor.cpp \
	DebugChecksumsSubsection.cpp \
	DebugCrossExSubsection.cpp \
	DebugCrossImpSubsection.cpp \
	DebugFrameDataSubsection.cpp \
	DebugInlineeLinesSubsection.cpp \
	DebugLinesSubsection.cpp \
	DebugStringTableSubsection.cpp \
	DebugSubsection.cpp \
	DebugSubsectionRecord.cpp \
	DebugSubsectionVisitor.cpp \
	DebugSymbolRVASubsection.cpp \
	DebugSymbolsSubsection.cpp \
	EnumTables.cpp \
	Formatters.cpp \
	GlobalTypeTableBuilder.cpp \
	LazyRandomTypeCollection.cpp \
	Line.cpp \
	MergingTypeTableBuilder.cpp \
	RecordName.cpp \
	RecordSerialization.cpp \
	SimpleTypeSerializer.cpp \
	StringsAndChecksums.cpp \
	SymbolRecordMapping.cpp \
	SymbolDumper.cpp \
	SymbolSerializer.cpp \
	TypeDumpVisitor.cpp \
	TypeIndex.cpp \
	TypeIndexDiscovery.cpp \
	TypeHashing.cpp \
	TypeRecordMapping.cpp \
	TypeStreamMerger.cpp \
	TypeTableCollection.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/DebugInfo/CodeView

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
