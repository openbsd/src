# $OpenBSD: Makefile,v 1.12 2023/11/11 18:35:36 robert Exp $

LIB=	LLVMDebugInfoCodeView
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/DebugInfo/CodeView

SRCS+=	AppendingTypeTableBuilder.cpp \
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
	DebugInfoCodeViewEnumTables.cpp \
	Formatters.cpp \
	GlobalTypeTableBuilder.cpp \
	LazyRandomTypeCollection.cpp \
	Line.cpp \
	MergingTypeTableBuilder.cpp \
	RecordName.cpp \
	RecordSerialization.cpp \
	SimpleTypeSerializer.cpp \
	StringsAndChecksums.cpp \
	SymbolDumper.cpp \
	SymbolRecordHelpers.cpp \
	SymbolRecordMapping.cpp \
	SymbolSerializer.cpp \
	TypeDumpVisitor.cpp \
	TypeIndex.cpp \
	TypeIndexDiscovery.cpp \
	TypeHashing.cpp \
	TypeRecordHelpers.cpp \
	TypeRecordMapping.cpp \
	TypeStreamMerger.cpp \
	TypeTableCollection.cpp

DebugInfoCodeViewEnumTables.cpp:
	ln -s ${LLVM_SRCS}/lib/DebugInfo/CodeView/EnumTables.cpp $@

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/DebugInfo/CodeView

CLEANFILES+= DebugInfoCodeViewEnumTables.cpp
