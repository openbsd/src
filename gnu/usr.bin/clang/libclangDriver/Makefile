# $OpenBSD: Makefile,v 1.2 2017/01/24 08:44:47 patrick Exp $

LIB=	clangDriver
NOPIC=
NOPROFILE=

CPPFLAGS+=	${CLANG_INCLUDES}

.include <bsd.own.mk>
SRCS=	Action.cpp \
	Compilation.cpp \
	CrossWindowsToolChain.cpp \
	Distro.cpp \
	Driver.cpp \
	DriverOptions.cpp \
	Job.cpp \
	MinGWToolChain.cpp \
	Multilib.cpp \
	MSVCToolChain.cpp \
	Phases.cpp \
	SanitizerArgs.cpp \
	Tool.cpp \
	ToolChain.cpp \
	ToolChains.cpp \
	Tools.cpp \
	Types.cpp

.PATH:	${.CURDIR}/../../../llvm/tools/clang/lib/Driver

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
