# $OpenBSD: Makefile,v 1.3 2019/06/23 22:07:43 patrick Exp $

LIB=	lldbCore
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	Address.cpp \
	AddressRange.cpp \
	AddressResolver.cpp \
	AddressResolverFileLine.cpp \
	AddressResolverName.cpp \
	Communication.cpp \
	Debugger.cpp \
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
	Mangled.cpp \
	Module.cpp \
	ModuleChild.cpp \
	ModuleList.cpp \
	Opcode.cpp \
	PluginManager.cpp \
	RichManglingContext.cpp \
	SearchFilter.cpp \
	Section.cpp \
	SourceManager.cpp \
	StreamAsynchronousIO.cpp \
	StreamFile.cpp \
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
	ValueObjectVariable.cpp \

.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Core

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
