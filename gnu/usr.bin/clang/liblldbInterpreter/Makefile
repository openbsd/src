# $OpenBSD: Makefile,v 1.3 2020/08/03 14:45:30 patrick Exp $

LIB=	lldbInterpreter
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	CommandAlias.cpp \
	CommandHistory.cpp \
	CommandInterpreter.cpp \
	CommandObject.cpp \
	CommandObjectRegexCommand.cpp \
	CommandObjectScript.cpp \
	CommandOptionValidators.cpp \
	CommandReturnObject.cpp \
	OptionArgParser.cpp \
	OptionGroupArchitecture.cpp \
	OptionGroupBoolean.cpp \
	OptionGroupFile.cpp \
	OptionGroupFormat.cpp \
	OptionGroupPythonClassWithDict.cpp \
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
	OptionValueFileSpecList.cpp \
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

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Interpreter

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
CPPFLAGS+=	-I${.OBJDIR}/../include/lldb/Interpreter
