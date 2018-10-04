# $OpenBSD: Makefile,v 1.1 2018/10/04 11:43:29 patrick Exp $

LIB=	lldbCommands
NOPIC=
NOPROFILE=

.include <bsd.own.mk>

SRCS=	CommandCompletions.cpp \
	CommandObjectApropos.cpp \
	CommandObjectArgs.cpp \
	CommandObjectBreakpoint.cpp \
	CommandObjectBreakpointCommand.cpp \
	CommandObjectBugreport.cpp \
	CommandObjectCommands.cpp \
	CommandObjectDisassemble.cpp \
	CommandObjectExpression.cpp \
	CommandObjectFrame.cpp \
	CommandObjectGUI.cpp \
	CommandObjectHelp.cpp \
	CommandObjectLog.cpp \
	CommandObjectMemory.cpp \
	CommandObjectMultiword.cpp \
	CommandObjectPlatform.cpp \
	CommandObjectPlugin.cpp \
	CommandObjectProcess.cpp \
	CommandObjectQuit.cpp \
	CommandObjectRegister.cpp \
	CommandObjectSettings.cpp \
	CommandObjectSource.cpp \
	CommandObjectSyntax.cpp \
	CommandObjectTarget.cpp \
	CommandObjectThread.cpp \
	CommandObjectType.cpp \
	CommandObjectVersion.cpp \
	CommandObjectWatchpoint.cpp \
	CommandObjectWatchpointCommand.cpp \
	CommandObjectLanguage.cpp \

.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Commands

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
