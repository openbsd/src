# $OpenBSD: Makefile,v 1.2 2019/06/23 22:07:44 patrick Exp $

LIB=	lldbPluginObjectFile
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	ELFHeader.cpp \
	ObjectFileBreakpad.cpp \
	ObjectFileELF.cpp \
	ObjectFileJIT.cpp \
	ObjectFileMachO.cpp \
	ObjectFilePECOFF.cpp \
	WindowsMiniDump.cpp \

.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/ObjectFile/Breakpad
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/ObjectFile/ELF
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/ObjectFile/JIT
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/ObjectFile/Mach-O
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/ObjectFile/PECOFF

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
