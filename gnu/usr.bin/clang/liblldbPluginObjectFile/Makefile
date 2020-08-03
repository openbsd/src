# $OpenBSD: Makefile,v 1.3 2020/08/03 14:45:30 patrick Exp $

LIB=	lldbPluginObjectFile
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	BreakpadRecords.cpp \
	ELFHeader.cpp \
	ObjectFileBreakpad.cpp \
	ObjectFileELF.cpp \
	ObjectFileJIT.cpp \
	ObjectFileMachO.cpp \
	ObjectFilePECOFF.cpp \
	PECallFrameInfo.cpp \
	WindowsMiniDump.cpp \

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/ObjectFile/Breakpad
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/ObjectFile/ELF
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/ObjectFile/JIT
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/ObjectFile/Mach-O
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/ObjectFile/PECOFF

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
