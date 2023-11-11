# $OpenBSD: Makefile,v 1.1 2023/11/11 18:35:37 robert Exp $

LIB=	LLVMObjCopy
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/ObjCopy
CPPFLAGS+=	-I${LLVM_SRCS}/lib/ObjCopy

SRCS+=	Archive.cpp \
	CommonConfig.cpp \
	ObjCopy.cpp \
	ConfigManager.cpp

# COFF
SRCS+=	COFFObjcopy.cpp \
	COFFObject.cpp \
	COFFReader.cpp \
	COFFWriter.cpp \

# ELF
SRCS+=	ELFObjcopy.cpp \
	ELFObject.cpp \

# MachO
SRCS+=	MachOObjcopy.cpp \
	MachOReader.cpp \
	MachOWriter.cpp \
	MachOLayoutBuilder.cpp \
	MachOObject.cpp \

# wasm
SRCS+=	WasmObject.cpp \
	WasmReader.cpp \
	WasmWriter.cpp \
	WasmObjcopy.cpp \

# XCOFF
SRCS+=	XCOFFObjcopy.cpp \
	XCOFFReader.cpp \
	XCOFFWriter.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ObjCopy
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ObjCopy/COFF
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ObjCopy/ELF
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ObjCopy/MachO
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ObjCopy/wasm
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ObjCopy/XCOFF

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
