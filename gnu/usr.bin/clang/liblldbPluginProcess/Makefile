# $OpenBSD: Makefile,v 1.6 2019/12/21 13:16:25 mortimer Exp $

LIB=	lldbPluginProcess
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	CrashReason.cpp \
	DynamicRegisterInfo.cpp \
	FreeBSDSignals.cpp \
	GDBRemoteClientBase.cpp \
	GDBRemoteCommunication.cpp \
	GDBRemoteCommunicationClient.cpp \
	GDBRemoteCommunicationHistory.cpp \
	GDBRemoteCommunicationReplayServer.cpp \
	GDBRemoteCommunicationServer.cpp \
	GDBRemoteCommunicationServerCommon.cpp \
	GDBRemoteCommunicationServerLLGS.cpp \
	GDBRemoteCommunicationServerPlatform.cpp \
	GDBRemoteRegisterContext.cpp \
	GDBRemoteSignals.cpp \
	HistoryThread.cpp \
	HistoryUnwind.cpp \
	InferiorCallPOSIX.cpp \
	LinuxProcMaps.cpp \
	LinuxSignals.cpp \
	MinidumpParser.cpp \
	MinidumpTypes.cpp \
	MipsLinuxSignals.cpp \
	NativeProcessOpenBSD.cpp \
	NativeRegisterContextOpenBSD.cpp \
	NativeRegisterContextOpenBSD_x86_64.cpp \
	NativeRegisterContextOpenBSD_arm64.cpp \
	NativeRegisterContextRegisterInfo.cpp \
	NativeThreadOpenBSD.cpp \
	NetBSDSignals.cpp \
	OpenBSDSignals.cpp \
	ProcessElfCore.cpp \
	ProcessGDBRemote.cpp \
	ProcessGDBRemoteLog.cpp \
	ProcessMachCore.cpp \
	ProcessMessage.cpp \
	ProcessMinidump.cpp \
	ProcessPOSIXLog.cpp \
	RegisterContextDarwin_arm.cpp \
	RegisterContextDarwin_arm64.cpp \
	RegisterContextDarwin_i386.cpp \
	RegisterContextDarwin_x86_64.cpp \
	RegisterContextDummy.cpp \
	RegisterContextFreeBSD_i386.cpp \
	RegisterContextFreeBSD_mips64.cpp \
	RegisterContextFreeBSD_powerpc.cpp \
	RegisterContextFreeBSD_x86_64.cpp \
	RegisterContextHistory.cpp \
	RegisterContextLLDB.cpp \
	RegisterContextLinux_i386.cpp \
	RegisterContextLinux_mips.cpp \
	RegisterContextLinux_mips64.cpp \
	RegisterContextLinux_s390x.cpp \
	RegisterContextLinux_x86_64.cpp \
	RegisterContextMacOSXFrameBackchain.cpp \
	RegisterContextMach_arm.cpp \
	RegisterContextMach_i386.cpp \
	RegisterContextMach_x86_64.cpp \
	RegisterContextMemory.cpp \
	RegisterContextMinidump_ARM.cpp \
	RegisterContextMinidump_ARM64.cpp \
	RegisterContextMinidump_x86_32.cpp \
	RegisterContextMinidump_x86_64.cpp \
	RegisterContextNetBSD_x86_64.cpp \
	RegisterContextOpenBSD_i386.cpp \
	RegisterContextOpenBSD_x86_64.cpp \
	RegisterContextPOSIXCore_arm.cpp \
	RegisterContextPOSIXCore_arm64.cpp \
	RegisterContextPOSIXCore_mips64.cpp \
	RegisterContextPOSIXCore_powerpc.cpp \
	RegisterContextPOSIXCore_ppc64le.cpp \
	RegisterContextPOSIXCore_s390x.cpp \
	RegisterContextPOSIXCore_x86_64.cpp \
	RegisterContextPOSIX_arm.cpp \
	RegisterContextPOSIX_arm64.cpp \
	RegisterContextPOSIX_mips64.cpp \
	RegisterContextPOSIX_powerpc.cpp \
	RegisterContextPOSIX_ppc64le.cpp \
	RegisterContextPOSIX_s390x.cpp \
	RegisterContextPOSIX_x86.cpp \
	RegisterContextThreadMemory.cpp \
	RegisterInfoPOSIX_arm.cpp \
	RegisterInfoPOSIX_arm64.cpp \
	RegisterInfoPOSIX_ppc64le.cpp \
	RegisterUtilities.cpp \
	StopInfoMachException.cpp \
	ThreadElfCore.cpp \
	ThreadGDBRemote.cpp \
	ThreadMachCore.cpp \
	ThreadMemory.cpp \
	ThreadMinidump.cpp \
	UnwindLLDB.cpp \
	UnwindMacOSXFrameBackchain.cpp \

.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/Process/Utility
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/Process/elf-core
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/Process/gdb-remote
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/Process/mach-core
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/Process/minidump
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/Process/POSIX
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/Process/OpenBSD

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
