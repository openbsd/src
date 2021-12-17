# $OpenBSD: Makefile,v 1.10 2021/12/17 14:55:47 patrick Exp $

LIB=	lldbPluginProcess
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
# Process/OpenBSD
SRCS=	NativeProcessOpenBSD.cpp \
	NativeRegisterContextOpenBSD.cpp \
	NativeRegisterContextOpenBSD_arch.cpp \
	NativeRegisterContextOpenBSD_arm64.cpp \
	NativeRegisterContextOpenBSD_x86_64.cpp \
	NativeThreadOpenBSD.cpp

# Process/POSIX
SRCS+=	CrashReason.cpp \
	NativeProcessELF.cpp \
	ProcessMessage.cpp \
	ProcessPOSIXLog.cpp

# Process/gdb-remote
SRCS+=	GDBRemoteClientBase.cpp \
	GDBRemoteCommunication.cpp \
	GDBRemoteCommunicationClient.cpp \
	GDBRemoteCommunicationHistory.cpp \
	GDBRemoteCommunicationReplayServer.cpp \
	GDBRemoteCommunicationServer.cpp \
	GDBRemoteCommunicationServerCommon.cpp \
	GDBRemoteCommunicationServerLLGS.cpp \
	GDBRemoteCommunicationServerPlatform.cpp \
	GDBRemoteRegisterContext.cpp \
	ProcessGDBRemote.cpp \
	ProcessGDBRemoteLog.cpp \
	ThreadGDBRemote.cpp

# Process/Utility
SRCS+=	AuxVector.cpp \
	DynamicRegisterInfo.cpp \
	FreeBSDSignals.cpp \
	GDBRemoteSignals.cpp \
	HistoryThread.cpp \
	HistoryUnwind.cpp \
	InferiorCallPOSIX.cpp \
	LinuxProcMaps.cpp \
	LinuxSignals.cpp \
	MemoryTagManagerAArch64MTE.cpp \
	MipsLinuxSignals.cpp \
	NativeProcessSoftwareSingleStep.cpp \
	NativeRegisterContextDBReg_arm64.cpp \
	NativeRegisterContextDBReg_x86.cpp \
	NativeRegisterContextRegisterInfo.cpp \
	NetBSDSignals.cpp \
	OpenBSDSignals.cpp \
	RegisterContext_x86.cpp \
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
	RegisterContextLinux_i386.cpp \
	RegisterContextLinux_x86_64.cpp \
	RegisterContextLinux_s390x.cpp \
	RegisterContextMach_arm.cpp \
	RegisterContextMach_i386.cpp \
	RegisterContextMach_x86_64.cpp \
	RegisterContextMemory.cpp \
	RegisterContextNetBSD_i386.cpp \
	RegisterContextNetBSD_x86_64.cpp \
	RegisterContextOpenBSD_i386.cpp \
	RegisterContextOpenBSD_x86_64.cpp \
	RegisterContextPOSIX_arm.cpp \
	RegisterContextPOSIX_arm64.cpp \
	RegisterContextPOSIX_mips64.cpp \
	RegisterContextPOSIX_powerpc.cpp \
	RegisterContextPOSIX_ppc64le.cpp \
	RegisterContextPOSIX_s390x.cpp \
	RegisterContextPOSIX_x86.cpp \
	RegisterContextThreadMemory.cpp \
	RegisterContextWindows_i386.cpp \
	RegisterContextWindows_x86_64.cpp \
	RegisterInfoPOSIX_arm.cpp \
	RegisterInfoPOSIX_arm64.cpp \
	RegisterInfoPOSIX_ppc64le.cpp \
	StopInfoMachException.cpp \
	ThreadMemory.cpp

# Process/elf-core
SRCS+=	ProcessElfCore.cpp \
	ThreadElfCore.cpp \
	RegisterContextPOSIXCore_arm.cpp \
	RegisterContextPOSIXCore_arm64.cpp \
	RegisterContextPOSIXCore_mips64.cpp \
	RegisterContextPOSIXCore_powerpc.cpp \
	RegisterContextPOSIXCore_ppc64le.cpp \
	RegisterContextPOSIXCore_s390x.cpp \
	RegisterContextPOSIXCore_x86_64.cpp \
	RegisterUtilities.cpp

# Process/mach-core
SRCS+=	ProcessMachCore.cpp \
	ThreadMachCore.cpp

# Process/minidump
SRCS+=	MinidumpTypes.cpp \
	MinidumpParser.cpp \
	RegisterContextMinidump_ARM.cpp \
	RegisterContextMinidump_ARM64.cpp \
	RegisterContextMinidump_x86_32.cpp \
	RegisterContextMinidump_x86_64.cpp \
	ProcessMinidump.cpp \
	ThreadMinidump.cpp

# Process/scripted
SRCS+=	ScriptedProcess.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Process/OpenBSD
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Process/POSIX
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Process/gdb-remote
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Process/Utility
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Process/elf-core
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Process/mach-core
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Process/minidump
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/Process/scripted

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
CPPFLAGS+=	-I${.OBJDIR}/../include/lldb/Plugins
