# $OpenBSD: Makefile,v 1.4 2021/04/28 12:55:42 patrick Exp $

LIB=	lldbPluginPlatform
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	AdbClient.cpp \
	PlatformAndroid.cpp \
	PlatformAndroidRemoteGDBServer.cpp \
	PlatformFreeBSD.cpp \
	PlatformLinux.cpp \
	PlatformDarwin.cpp \
	PlatformDarwinKernel.cpp \
	PlatformMacOSX.cpp \
	PlatformRemoteiOS.cpp \
	PlatformRemoteAppleTV.cpp \
	PlatformRemoteAppleWatch.cpp \
	PlatformRemoteDarwinDevice.cpp \
	PlatformRemoteAppleBridge.cpp \
	PlatformNetBSD.cpp \
	PlatformOpenBSD.cpp \
	PlatformPOSIX.cpp \
	PlatformWindows.cpp \
	PlatformRemoteGDBServer.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Platform/Android
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Platform/FreeBSD
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Platform/Linux
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Platform/MacOSX
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Platform/NetBSD
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Platform/OpenBSD
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Platform/POSIX
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Platform/Windows
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Platform/gdb-server

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
