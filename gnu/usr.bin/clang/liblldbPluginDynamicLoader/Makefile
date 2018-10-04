# $OpenBSD: Makefile,v 1.1 2018/10/04 11:43:29 patrick Exp $

LIB=	lldbPluginDynamicLoader
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	AuxVector.cpp \
	DYLDRendezvous.cpp \
	DynamicLoaderDarwin.cpp \
	DynamicLoaderHexagonDYLD.cpp \
	DynamicLoaderMacOS.cpp \
	DynamicLoaderMacOSXDYLD.cpp \
	DynamicLoaderPOSIXDYLD.cpp \
	DynamicLoaderStatic.cpp \
	DynamicLoaderWindowsDYLD.cpp \
	HexagonDYLDRendezvous.cpp \

.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/DynamicLoader/Hexagon-DYLD
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/DynamicLoader/MacOSX-DYLD
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/DynamicLoader/POSIX-DYLD
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/DynamicLoader/Static
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/DynamicLoader/Windows-DYLD

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
