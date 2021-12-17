# $OpenBSD: Makefile,v 1.5 2021/12/17 14:55:46 patrick Exp $

LIB=	lldbPluginObjectFile
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	BreakpadRecords.cpp \
	ObjectFileBreakpad.cpp \
	ELFHeader.cpp \
	ObjectFileELF.cpp \
	ObjectFileJIT.cpp \
	ObjectFileMachO.cpp \
	ObjectFilePDB.cpp \
	ObjectFilePECOFF.cpp \
	PECallFrameInfo.cpp \
	WindowsMiniDump.cpp \
	ObjectFileWasm.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/ObjectFile/Breakpad
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/ObjectFile/ELF
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/ObjectFile/Mach-O
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/ObjectFile/PDB
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/ObjectFile/PECOFF
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/ObjectFile/JIT
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/ObjectFile/wasm

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
