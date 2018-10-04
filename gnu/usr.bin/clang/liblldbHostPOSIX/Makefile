# $OpenBSD: Makefile,v 1.1 2018/10/04 11:43:29 patrick Exp $

LIB=	lldbHostPOSIX
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	ConnectionFileDescriptorPosix.cpp \
	DomainSocket.cpp \
	FileSystem.cpp \
	HostInfoPosix.cpp \
	HostProcessPosix.cpp \
	HostThreadPosix.cpp \
	LockFilePosix.cpp \
	PipePosix.cpp \
	ProcessLauncherPosixFork.cpp \

.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Host/posix

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
