# $OpenBSD: Makefile,v 1.3 2017/07/09 15:28:36 espie Exp $

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

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
