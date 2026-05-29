# $OpenBSD: Makefile,v 1.9 2026/05/29 11:18:31 robert Exp $

LIB=	lldbCore
NOPIC=
NOPROFILE=

.include <bsd.own.mk>

SRCS=	Address.cpp \
	AddressRange.cpp \
	AddressRangeListImpl.cpp \
	AddressResolver.cpp \
	AddressResolverFileLine.cpp \
	Communication.cpp \
	DataFileCache.cpp \
	Debugger.cpp \
	DebuggerEvents.cpp \
	Declaration.cpp \
	DemangledNameInfo.cpp \
	Disassembler.cpp \
	DumpDataExtractor.cpp \
	DumpRegisterValue.cpp \
	DumpRegisterInfo.cpp \
	DynamicLoader.cpp \
	EmulateInstruction.cpp \
	FileLineResolver.cpp \
	FormatEntity.cpp \
	Highlighter.cpp \
	IOHandler.cpp \
	IOHandlerCursesGUI.cpp \
	Mangled.cpp \
	Module.cpp \
	ModuleChild.cpp \
	ModuleList.cpp \
	Opcode.cpp \
	PluginManager.cpp \
	Progress.cpp \
	ProtocolServer.cpp \
	Statusline.cpp \
	RichManglingContext.cpp \
	SearchFilter.cpp \
	Section.cpp \
	SourceLocationSpec.cpp \
	SourceManager.cpp \
	StreamAsynchronousIO.cpp \
	Telemetry.cpp \
	ThreadedCommunication.cpp \
	UserSettingsController.cpp \
	Value.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Core

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
CPPFLAGS+=	-I${.OBJDIR}/../include/lldb/Core
