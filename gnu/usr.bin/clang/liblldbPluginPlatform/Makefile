# $OpenBSD: Makefile,v 1.1 2018/10/04 11:43:29 patrick Exp $

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
	PlatformKalimba.cpp \
	PlatformLinux.cpp \
	PlatformMacOSX.cpp \
	PlatformNetBSD.cpp \
	PlatformOpenBSD.cpp \
	PlatformPOSIX.cpp \
	PlatformRemoteAppleTV.cpp \
	PlatformRemoteAppleWatch.cpp \
	PlatformRemoteDarwinDevice.cpp \
	PlatformRemoteGDBServer.cpp \
	PlatformRemoteiOS.cpp \
	PlatformWindows.cpp \
	PlatformiOSSimulator.cpp \

.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/Platform/Android
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/Platform/FreeBSD
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/Platform/Kalimba
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/Platform/Linux
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/Platform/MacOSX
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/Platform/NetBSD
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/Platform/OpenBSD
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/Platform/POSIX
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/Platform/Windows
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/Platform/gdb-server

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
