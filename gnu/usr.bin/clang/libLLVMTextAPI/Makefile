# $OpenBSD: Makefile,v 1.4 2025/06/11 17:46:32 robert Exp $

LIB=	LLVMTextAPI
NOPROFILE=

# TextAPI
SRCS+=	Architecture.cpp \
	ArchitectureSet.cpp \
	InterfaceFile.cpp \
	TextStubV5.cpp \
	PackedVersion.cpp \
	Platform.cpp \
	RecordsSlice.cpp \
	RecordVisitor.cpp \
	Symbol.cpp \
	SymbolSet.cpp \
	TextAPITarget.cpp \
	TextAPIError.cpp \
	TextStub.cpp \
	TextStubCommon.cpp \
	TextAPIUtils.cpp

TextAPITarget.cpp:
	ln -s ${LLVM_SRCS}/lib/TextAPI/Target.cpp $@

TextAPIUtils.cpp:
	ln -s ${LLVM_SRCS}/lib/TextAPI/Utils.cpp $@

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/TextAPI

CLEANFILES+= TextAPITarget.cpp
