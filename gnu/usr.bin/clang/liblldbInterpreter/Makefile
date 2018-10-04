# $OpenBSD: Makefile,v 1.1 2018/10/04 11:43:29 patrick Exp $

LIB=	lldbInterpreter
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	Args.cpp \
	CommandAlias.cpp \
	CommandHistory.cpp \
	CommandInterpreter.cpp \
	CommandObject.cpp \
	CommandObjectRegexCommand.cpp \
	CommandObjectScript.cpp \
	CommandOptionValidators.cpp \
	CommandReturnObject.cpp \
	OptionGroupArchitecture.cpp \
	OptionGroupBoolean.cpp \
	OptionGroupFile.cpp \
	OptionGroupFormat.cpp \
	OptionGroupOutputFile.cpp \
	OptionGroupPlatform.cpp \
	OptionGroupString.cpp \
	OptionGroupUInt64.cpp \
	OptionGroupUUID.cpp \
	OptionGroupValueObjectDisplay.cpp \
	OptionValue.cpp \
	OptionValueArch.cpp \
	OptionValueArgs.cpp \
	OptionValueArray.cpp \
	OptionValueBoolean.cpp \
	OptionValueChar.cpp \
	OptionValueDictionary.cpp \
	OptionValueEnumeration.cpp \
	OptionValueFileSpec.cpp \
	OptionValueFileSpecLIst.cpp \
	OptionValueFormat.cpp \
	OptionValueFormatEntity.cpp \
	OptionValueLanguage.cpp \
	OptionValuePathMappings.cpp \
	OptionValueProperties.cpp \
	OptionValueRegex.cpp \
	OptionValueSInt64.cpp \
	OptionValueString.cpp \
	OptionValueUInt64.cpp \
	OptionValueUUID.cpp \
	OptionGroupVariable.cpp \
	OptionGroupWatchpoint.cpp \
	Options.cpp \
	Property.cpp \
	ScriptInterpreter.cpp \

.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Interpreter

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
