# $OpenBSD: Makefile,v 1.6 2025/06/11 17:46:33 robert Exp $

LIB=	lldbBreakpoint
NOPIC=
NOPROFILE=

.include <bsd.own.mk>

SRCS=	Breakpoint.cpp \
	BreakpointID.cpp \
	BreakpointIDList.cpp \
	BreakpointList.cpp \
	BreakpointLocation.cpp \
	BreakpointLocationCollection.cpp \
	BreakpointLocationList.cpp \
	BreakpointName.cpp \
	BreakpointOptions.cpp \
	BreakpointPrecondition.cpp \
	BreakpointResolver.cpp \
	BreakpointResolverAddress.cpp \
	BreakpointResolverFileLine.cpp \
	BreakpointResolverFileRegex.cpp \
	BreakpointResolverName.cpp \
	BreakpointResolverScripted.cpp \
	BreakpointSite.cpp \
	Stoppoint.cpp \
	StoppointCallbackContext.cpp \
	StoppointSite.cpp \
	StopPointSiteList.cpp \
	Watchpoint.cpp \
	WatchpointAlgorithms.cpp \
	WatchpointList.cpp \
	WatchpointOptions.cpp \
	WatchpointResource.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Breakpoint

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
