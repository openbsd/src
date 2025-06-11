# $OpenBSD: Makefile,v 1.7 2025/06/11 17:46:33 robert Exp $

LIB=	lldbPluginObjectFile
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	BreakpadRecords.cpp \
	ObjectFileBreakpad.cpp \
	ELFHeader.cpp \
	ObjectFileCOFF.cpp \
	ObjectFileELF.cpp \
	ObjectFileJSON.cpp \
	ObjectFileMachO.cpp \
	ObjectFileMinidump.cpp \
	ObjectFilePDB.cpp \
	ObjectFilePECOFF.cpp \
	ObjectFilePlaceholder.cpp \
	MinidumpFileBuilder.cpp \
	PECallFrameInfo.cpp \
	WindowsMiniDump.cpp \
	ObjectFileWasm.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/ObjectFile/Breakpad
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/ObjectFile/COFF
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/ObjectFile/ELF
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/ObjectFile/JSON
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/ObjectFile/Mach-O
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/ObjectFile/Minidump
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/ObjectFile/PDB
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/ObjectFile/PECOFF
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/ObjectFile/Placeholder
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/ObjectFile/wasm

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
CPPFLAGS+=	-I${.OBJDIR}/../include/lldb/Plugins
