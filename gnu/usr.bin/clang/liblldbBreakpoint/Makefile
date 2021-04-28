# $OpenBSD: Makefile,v 1.4 2021/04/28 12:55:41 patrick Exp $

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
	BreakpointSiteList.cpp \
	Stoppoint.cpp \
	StoppointCallbackContext.cpp \
	StoppointLocation.cpp \
	Watchpoint.cpp \
	WatchpointList.cpp \
	WatchpointOptions.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Breakpoint

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
