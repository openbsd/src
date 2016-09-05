# $OpenBSD: Makefile,v 1.1 2016/09/05 10:56:49 pascal Exp $

LIB=	LLVMObject
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Object

.include <bsd.own.mk>
SRCS=	Archive.cpp \
	ArchiveWriter.cpp \
	Binary.cpp \
	COFFObjectFile.cpp \
	COFFYAML.cpp \
	ELF.cpp \
	ELFObjectFile.cpp \
	ELFYAML.cpp \
	Error.cpp \
	IRObjectFile.cpp \
	MachOObjectFile.cpp \
	MachOUniversal.cpp \
	Object.cpp \
	ObjectFile.cpp \
	RecordStreamer.cpp \
	SymbolicFile.cpp \
	SymbolSize.cpp \
	FunctionIndexObjectFile.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Object

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
