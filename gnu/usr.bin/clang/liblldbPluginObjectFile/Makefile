# $OpenBSD: Makefile,v 1.6 2023/11/11 18:35:39 robert Exp $

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
CPPFLAGS+=	-I${.OBJDIR}/../include/lldb/Plugins
