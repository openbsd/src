# $OpenBSD: Makefile,v 1.2 2021/12/17 14:55:47 patrick Exp $

.include <bsd.own.mk>

PROG=	llvm-objcopy
BINDIR=	/usr/bin
LIBEXECDIR=/usr/libexec

SRCS=	ConfigManager.cpp \
	llvm-objcopy.cpp \
	COFFObjcopy.cpp \
	COFFObject.cpp \
	COFFReader.cpp \
	COFFWriter.cpp \
	ELFObjcopy.cpp \
	ELFObject.cpp \
	MachOObjcopy.cpp \
	MachOReader.cpp \
	MachOWriter.cpp \
	MachOLayoutBuilder.cpp \
	MachOObject.cpp \
	WasmObject.cpp \
	WasmReader.cpp \
	WasmWriter.cpp \
	WasmObjcopy.cpp

COFFObject.cpp:
	ln -s ${.CURDIR}/../../../llvm/llvm/tools/llvm-objcopy/COFF/Object.cpp $@

COFFReader.cpp:
	ln -s ${.CURDIR}/../../../llvm/llvm/tools/llvm-objcopy/COFF/Reader.cpp $@

COFFWriter.cpp:
	ln -s ${.CURDIR}/../../../llvm/llvm/tools/llvm-objcopy/COFF/Writer.cpp $@

ELFObject.cpp:
	ln -s ${.CURDIR}/../../../llvm/llvm/tools/llvm-objcopy/ELF/Object.cpp $@

MachOObject.cpp:
	ln -s ${.CURDIR}/../../../llvm/llvm/tools/llvm-objcopy/MachO/Object.cpp $@

WasmObject.cpp:
	ln -s ${.CURDIR}/../../../llvm/llvm/tools/llvm-objcopy/wasm/Object.cpp $@

WasmReader.cpp:
	ln -s ${.CURDIR}/../../../llvm/llvm/tools/llvm-objcopy/wasm/Reader.cpp $@

WasmWriter.cpp:
	ln -s ${.CURDIR}/../../../llvm/llvm/tools/llvm-objcopy/wasm/Writer.cpp $@

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm-objcopy
CPPFLAGS+=	-I${.CURDIR}/../../../llvm/llvm/tools/llvm-objcopy
CPPFLAGS+=	${CPPFLAGS_$*}

CPPFLAGS_COFFObject=	-I${.CURDIR}/../../../llvm/llvm/tools/llvm-objcopy/COFF
CPPFLAGS_COFFReader=	-I${.CURDIR}/../../../llvm/llvm/tools/llvm-objcopy/COFF
CPPFLAGS_COFFWriter=	-I${.CURDIR}/../../../llvm/llvm/tools/llvm-objcopy/COFF
CPPFLAGS_ELFObject=	-I${.CURDIR}/../../../llvm/llvm/tools/llvm-objcopy/ELF
CPPFLAGS_MachOObject=	-I${.CURDIR}/../../../llvm/llvm/tools/llvm-objcopy/MachO
CPPFLAGS_WasmObject=	-I${.CURDIR}/../../../llvm/llvm/tools/llvm-objcopy/wasm
CPPFLAGS_WasmReader=	-I${.CURDIR}/../../../llvm/llvm/tools/llvm-objcopy/wasm
CPPFLAGS_WasmWriter=	-I${.CURDIR}/../../../llvm/llvm/tools/llvm-objcopy/wasm

.PATH:	${.CURDIR}/../../../llvm/llvm/tools/llvm-objcopy
.PATH:	${.CURDIR}/../../../llvm/llvm/tools/llvm-objcopy/COFF
.PATH:	${.CURDIR}/../../../llvm/llvm/tools/llvm-objcopy/ELF
.PATH:	${.CURDIR}/../../../llvm/llvm/tools/llvm-objcopy/MachO
.PATH:	${.CURDIR}/../../../llvm/llvm/tools/llvm-objcopy/wasm

LLVM_LIBDEPS=	LLVM

LDADD+= -L ${.OBJDIR}/../libLLVM -lLLVM

CLEANFILES+=	COFFObject.cpp COFFReader.cpp COFFWriter.cpp
CLEANFILES+=	ELFObject.cpp MachOObject.cpp
CLEANFILES+=	WasmObject.cpp WasmReader.cpp WasmWriter.cpp

.include <bsd.prog.mk>
