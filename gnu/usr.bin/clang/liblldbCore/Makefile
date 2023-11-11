# $OpenBSD: Makefile,v 1.7 2023/11/11 18:35:39 robert Exp $

LIB=	lldbCore
NOPIC=
NOPROFILE=

.include <bsd.own.mk>

SRCS=	Address.cpp \
	AddressRange.cpp \
	AddressResolver.cpp \
	AddressResolverFileLine.cpp \
	Communication.cpp \
	DataFileCache.cpp \
	Debugger.cpp \
	DebuggerEvents.cpp \
	Declaration.cpp \
	Disassembler.cpp \
	DumpDataExtractor.cpp \
	DumpRegisterValue.cpp \
	DynamicLoader.cpp \
	EmulateInstruction.cpp \
	FileLineResolver.cpp \
	FileSpecList.cpp \
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
	RichManglingContext.cpp \
	SearchFilter.cpp \
	Section.cpp \
	SourceLocationSpec.cpp \
	SourceManager.cpp \
	StreamAsynchronousIO.cpp \
	StreamFile.cpp \
	ThreadedCommunication.cpp \
	UserSettingsController.cpp \
	Value.cpp \
	ValueObject.cpp \
	ValueObjectCast.cpp \
	ValueObjectChild.cpp \
	ValueObjectConstResult.cpp \
	ValueObjectConstResultCast.cpp \
	ValueObjectConstResultChild.cpp \
	ValueObjectConstResultImpl.cpp \
	ValueObjectDynamicValue.cpp \
	ValueObjectList.cpp \
	ValueObjectMemory.cpp \
	ValueObjectRegister.cpp \
	ValueObjectSyntheticFilter.cpp \
	ValueObjectUpdater.cpp \
	ValueObjectVariable.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Core

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
CPPFLAGS+=	-I${.OBJDIR}/../include/lldb/Core
