# $OpenBSD: Makefile,v 1.3 2020/08/03 14:45:30 patrick Exp $

LIB=	lldbPluginPlatform
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	AdbClient.cpp \
	PlatformAndroid.cpp \
	PlatformAndroidRemoteGDBServer.cpp \
	PlatformAppleSimulator.cpp \
	PlatformAppleTVSimulator.cpp \
	PlatformAppleWatchSimulator.cpp \
	PlatformDarwin.cpp \
	PlatformDarwinKernel.cpp \
	PlatformFreeBSD.cpp \
	PlatformLinux.cpp \
	PlatformMacOSX.cpp \
	PlatformNetBSD.cpp \
	PlatformOpenBSD.cpp \
	PlatformPOSIX.cpp \
	PlatformRemoteAppleBridge.cpp \
	PlatformRemoteAppleTV.cpp \
	PlatformRemoteAppleWatch.cpp \
	PlatformRemoteDarwinDevice.cpp \
	PlatformRemoteGDBServer.cpp \
	PlatformRemoteiOS.cpp \
	PlatformWindows.cpp \
	PlatformiOSSimulator.cpp \

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
