# $OpenBSD: Makefile,v 1.5 2021/12/17 14:55:46 patrick Exp $

LIB=	lldbInterpreter
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	CommandAlias.cpp \
	CommandHistory.cpp \
	CommandInterpreter.cpp \
	CommandObject.cpp \
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
	OptionValueFileColonLine.cpp \
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
	ScriptInterpreter.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Interpreter

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
CPPFLAGS+=	-I${.OBJDIR}/../include/lldb/Interpreter
