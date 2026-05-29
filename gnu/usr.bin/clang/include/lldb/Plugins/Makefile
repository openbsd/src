# $OpenBSD: Makefile,v 1.5 2026/05/29 11:18:30 robert Exp $

.include <bsd.own.mk>

LLDB_SRCS=	${.CURDIR}/../../../../../llvm/lldb

DEFS=	DynamicLoaderDarwinKernelProperties.inc \
	DynamicLoaderDarwinKernelPropertiesEnum.inc \
	JITLoaderGDBProperties.inc \
	JITLoaderGDBPropertiesEnum.inc \
	LanguageCPlusPlusProperties.inc \
	LanguageCPlusPlusPropertiesEnum.inc \
	ObjectFilePECOFFProperties.inc \
	ObjectFilePECOFFPropertiesEnum.inc \
	PlatformAndroidProperties.inc \
	PlatformAndroidPropertiesEnum.inc \
	PlatformMacOSXProperties.inc \
	PlatformMacOSXPropertiesEnum.inc \
	ProcessGDBRemoteProperties.inc \
	ProcessGDBRemotePropertiesEnum.inc \
	StructuredDataDarwinLogProperties.inc \
	StructuredDataDarwinLogPropertiesEnum.inc \
	SymbolFilePDBProperties.inc \
	SymbolFilePDBPropertiesEnum.inc \
	SymbolFileDWARFProperties.inc \
	SymbolFileDWARFPropertiesEnum.inc

all: ${DEFS}

install:
	@# Nothing here so far ...

clean cleandir:
	rm -f ${DEFS}

DynamicLoaderDarwinKernelProperties.inc: \
	${LLDB_SRCS}/source/Plugins/DynamicLoader/Darwin-Kernel/DynamicLoaderDarwinKernelProperties.td
	${.OBJDIR}/../../../lldb-tblgen/lldb-tblgen -gen-lldb-property-defs \
		-I${LLDB_SRCS}/source/Plugins/DynamicLoader/Darwin-Kernel \
		-o ${.TARGET} ${.ALLSRC}

DynamicLoaderDarwinKernelPropertiesEnum.inc: \
	${LLDB_SRCS}/source/Plugins/DynamicLoader/Darwin-Kernel/DynamicLoaderDarwinKernelProperties.td
	${.OBJDIR}/../../../lldb-tblgen/lldb-tblgen -gen-lldb-property-enum-defs \
		-I${LLDB_SRCS}/source/Plugins/DynamicLoader/Darwin-Kernel \
		-o ${.TARGET} ${.ALLSRC}

LanguageCPlusPlusProperties.inc: \
	${LLDB_SRCS}/source/Plugins/Language/CPlusPlus/LanguageCPlusPlusProperties.td
	${.OBJDIR}/../../../lldb-tblgen/lldb-tblgen -gen-lldb-property-defs \
		-I${LLDB_SRCS}/source/Plugins/Language/CPlusPlus \
		-o ${.TARGET} ${.ALLSRC}

LanguageCPlusPlusPropertiesEnum.inc: \
	${LLDB_SRCS}/source/Plugins/Language/CPlusPlus/LanguageCPlusPlusProperties.td
	${.OBJDIR}/../../../lldb-tblgen/lldb-tblgen -gen-lldb-property-enum-defs \
		-I${LLDB_SRCS}/source/Plugins/Language/CPlusPlus \
		-o ${.TARGET} ${.ALLSRC}

JITLoaderGDBProperties.inc: \
	${LLDB_SRCS}/source/Plugins/JITLoader/GDB/JITLoaderGDBProperties.td
	${.OBJDIR}/../../../lldb-tblgen/lldb-tblgen -gen-lldb-property-defs \
		-I${LLDB_SRCS}/source/Plugins/JITLoader/GDB \
		-o ${.TARGET} ${.ALLSRC}

JITLoaderGDBPropertiesEnum.inc: \
	${LLDB_SRCS}/source/Plugins/JITLoader/GDB/JITLoaderGDBProperties.td
	${.OBJDIR}/../../../lldb-tblgen/lldb-tblgen -gen-lldb-property-enum-defs \
		-I${LLDB_SRCS}/source/Plugins/JITLoader/GDB \
		-o ${.TARGET} ${.ALLSRC}

ObjectFilePECOFFProperties.inc: \
	${LLDB_SRCS}/source/Plugins/ObjectFile/PECOFF/ObjectFilePECOFFProperties.td
	${.OBJDIR}/../../../lldb-tblgen/lldb-tblgen -gen-lldb-property-defs \
		-I${LLDB_SRCS}/source/Plugins/ObjectFile/PECOFF \
		-o ${.TARGET} ${.ALLSRC}

ObjectFilePECOFFPropertiesEnum.inc: \
	${LLDB_SRCS}/source/Plugins/ObjectFile/PECOFF/ObjectFilePECOFFProperties.td
	${.OBJDIR}/../../../lldb-tblgen/lldb-tblgen -gen-lldb-property-enum-defs \
		-I${LLDB_SRCS}/source/Plugins/ObjectFile/PECOFF \
		-o ${.TARGET} ${.ALLSRC}

PlatformAndroidProperties.inc: \
	${LLDB_SRCS}/source/Plugins/Platform/Android/PlatformAndroidProperties.td
	${.OBJDIR}/../../../lldb-tblgen/lldb-tblgen -gen-lldb-property-defs \
		-I${LLDB_SRCS}/source/Plugins/Platform/Android \
		-o ${.TARGET} ${.ALLSRC}

PlatformAndroidPropertiesEnum.inc: \
	${LLDB_SRCS}/source/Plugins/Platform/Android/PlatformAndroidProperties.td
	${.OBJDIR}/../../../lldb-tblgen/lldb-tblgen -gen-lldb-property-enum-defs \
		-I${LLDB_SRCS}/source/Plugins/Platform/Android \
		-o ${.TARGET} ${.ALLSRC}

PlatformMacOSXProperties.inc: \
	${LLDB_SRCS}/source/Plugins/Platform/MacOSX/PlatformMacOSXProperties.td
	${.OBJDIR}/../../../lldb-tblgen/lldb-tblgen -gen-lldb-property-defs \
		-I${LLDB_SRCS}/source/Plugins/Platform/MacOSX \
		-o ${.TARGET} ${.ALLSRC}

PlatformMacOSXPropertiesEnum.inc: \
	${LLDB_SRCS}/source/Plugins/Platform/MacOSX/PlatformMacOSXProperties.td
	${.OBJDIR}/../../../lldb-tblgen/lldb-tblgen -gen-lldb-property-enum-defs \
		-I${LLDB_SRCS}/source/Plugins/Platform/MacOSX \
		-o ${.TARGET} ${.ALLSRC}

ProcessGDBRemoteProperties.inc: \
	${LLDB_SRCS}/source/Plugins/Process/gdb-remote/ProcessGDBRemoteProperties.td
	${.OBJDIR}/../../../lldb-tblgen/lldb-tblgen -gen-lldb-property-defs \
		-I${LLDB_SRCS}/source/Plugins/Process/gdb-remote \
		-o ${.TARGET} ${.ALLSRC}

ProcessGDBRemotePropertiesEnum.inc: \
	${LLDB_SRCS}/source/Plugins/Process/gdb-remote/ProcessGDBRemoteProperties.td
	${.OBJDIR}/../../../lldb-tblgen/lldb-tblgen -gen-lldb-property-enum-defs \
		-I${LLDB_SRCS}/source/Plugins/Process/gdb-remote \
		-o ${.TARGET} ${.ALLSRC}

StructuredDataDarwinLogProperties.inc: \
	${LLDB_SRCS}/source/Plugins/StructuredData/DarwinLog/StructuredDataDarwinLogProperties.td
	${.OBJDIR}/../../../lldb-tblgen/lldb-tblgen -gen-lldb-property-defs \
		-I${LLDB_SRCS}/source/Plugins/StructuredData/DarwinLog \
		-o ${.TARGET} ${.ALLSRC}

StructuredDataDarwinLogPropertiesEnum.inc: \
	${LLDB_SRCS}/source/Plugins/StructuredData/DarwinLog/StructuredDataDarwinLogProperties.td
	${.OBJDIR}/../../../lldb-tblgen/lldb-tblgen -gen-lldb-property-enum-defs \
		-I${LLDB_SRCS}/source/Plugins/StructuredData/DarwinLog \
		-o ${.TARGET} ${.ALLSRC}

SymbolFilePDBProperties.inc: \
	${LLDB_SRCS}/source/Plugins/SymbolFile/PDB/SymbolFilePDBProperties.td
	${.OBJDIR}/../../../lldb-tblgen/lldb-tblgen -gen-lldb-property-defs \
		-I${LLDB_SRCS}/source/Plugins/SymbolFile/PDB \
		-o ${.TARGET} ${.ALLSRC}

SymbolFilePDBPropertiesEnum.inc: \
	${LLDB_SRCS}/source/Plugins/SymbolFile/PDB/SymbolFilePDBProperties.td
	${.OBJDIR}/../../../lldb-tblgen/lldb-tblgen -gen-lldb-property-enum-defs \
		-I${LLDB_SRCS}/source/Plugins/SymbolFile/PDB \
		-o ${.TARGET} ${.ALLSRC}

SymbolFileDWARFProperties.inc: \
	${LLDB_SRCS}/source/Plugins/SymbolFile/DWARF/SymbolFileDWARFProperties.td
	${.OBJDIR}/../../../lldb-tblgen/lldb-tblgen -gen-lldb-property-defs \
		-I${LLDB_SRCS}/source/Plugins/SymbolFile/DWARF \
		-o ${.TARGET} ${.ALLSRC}

SymbolFileDWARFPropertiesEnum.inc: \
	${LLDB_SRCS}/source/Plugins/SymbolFile/DWARF/SymbolFileDWARFProperties.td
	${.OBJDIR}/../../../lldb-tblgen/lldb-tblgen -gen-lldb-property-enum-defs \
		-I${LLDB_SRCS}/source/Plugins/SymbolFile/DWARF \
		-o ${.TARGET} ${.ALLSRC}

.include <bsd.obj.mk>
