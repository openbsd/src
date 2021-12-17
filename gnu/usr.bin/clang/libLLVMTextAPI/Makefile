# $OpenBSD: Makefile,v 1.3 2021/12/17 14:55:46 patrick Exp $

LIB=	LLVMTextAPI
NOPROFILE=

# TextAPI
SRCS+=	Architecture.cpp \
	ArchitectureSet.cpp \
	InterfaceFile.cpp \
	PackedVersion.cpp \
	Platform.cpp \
	Symbol.cpp \
	TextAPITarget.cpp \
	TextStub.cpp \
	TextStubCommon.cpp

TextAPITarget.cpp:
	ln -s ${LLVM_SRCS}/lib/TextAPI/Target.cpp $@

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/TextAPI

CLEANFILES+= TextAPITarget.cpp
