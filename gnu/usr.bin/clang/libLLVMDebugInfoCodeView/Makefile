# $OpenBSD: Makefile,v 1.8 2019/03/05 09:44:40 jsg Exp $

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

DebugInfoCodeViewEnumTables.cpp:
	ln -s ${LLVM_SRCS}/lib/DebugInfo/CodeView/EnumTables.cpp $@

.PATH:	${.CURDIR}/../../../llvm/lib/DebugInfo/CodeView

CLEANFILES+= DebugInfoCodeViewEnumTables.cpp
