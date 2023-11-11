# $OpenBSD: Makefile,v 1.7 2023/11/11 18:35:39 robert Exp $

LIB=	lldbCommands
NOPIC=
NOPROFILE=

.include <bsd.own.mk>

SRCS=	CommandCompletions.cpp \
	CommandObjectApropos.cpp \
	CommandObjectBreakpoint.cpp \
	CommandObjectBreakpointCommand.cpp \
	CommandObjectCommands.cpp \
	CommandObjectDiagnostics.cpp \
	CommandObjectDisassemble.cpp \
	CommandObjectDWIMPrint.cpp \
	CommandObjectExpression.cpp \
	CommandObjectFrame.cpp \
	CommandObjectGUI.cpp \
	CommandObjectHelp.cpp \
	CommandObjectLanguage.cpp \
	CommandObjectLog.cpp \
	CommandObjectMemory.cpp \
	CommandObjectMemoryTag.cpp \
	CommandObjectMultiword.cpp \
	CommandObjectPlatform.cpp \
	CommandObjectPlugin.cpp \
	CommandObjectProcess.cpp \
	CommandObjectQuit.cpp \
	CommandObjectRegexCommand.cpp \
	CommandObjectRegister.cpp \
	CommandObjectScript.cpp \
	CommandObjectSession.cpp \
	CommandObjectSettings.cpp \
	CommandObjectSource.cpp \
	CommandObjectStats.cpp \
	CommandObjectTarget.cpp \
	CommandObjectThread.cpp \
	CommandObjectThreadUtil.cpp \
	CommandObjectTrace.cpp \
	CommandObjectType.cpp \
	CommandObjectVersion.cpp \
	CommandObjectWatchpoint.cpp \
	CommandObjectWatchpointCommand.cpp \
	CommandOptionArgumentTable.cpp \
	CommandOptionsProcessLaunch.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Commands

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
CPPFLAGS+=	-I${.OBJDIR}/../include/lldb/Commands
