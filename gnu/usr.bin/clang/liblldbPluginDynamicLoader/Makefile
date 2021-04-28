# $OpenBSD: Makefile,v 1.4 2021/04/28 12:55:41 patrick Exp $

LIB=	lldbPluginDynamicLoader
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	DynamicLoaderDarwinKernel.cpp \
	DynamicLoaderMacOSXDYLD.cpp \
	DynamicLoaderMacOS.cpp \
	DynamicLoaderDarwin.cpp \
	DYLDRendezvous.cpp \
	DynamicLoaderPOSIXDYLD.cpp \
	DynamicLoaderStatic.cpp \
	HexagonDYLDRendezvous.cpp \
	DynamicLoaderHexagonDYLD.cpp \
	DynamicLoaderWindowsDYLD.cpp \
	DynamicLoaderWasmDYLD.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/DynamicLoader/Darwin-Kernel
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/DynamicLoader/MacOSX-DYLD
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/DynamicLoader/POSIX-DYLD
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/DynamicLoader/Static
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/DynamicLoader/Hexagon-DYLD
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/DynamicLoader/Windows-DYLD
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/DynamicLoader/wasm-DYLD

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
CPPFLAGS+=	-I${.OBJDIR}/../include/lldb/Plugins
