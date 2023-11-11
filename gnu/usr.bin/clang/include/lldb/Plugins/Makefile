# $OpenBSD: Makefile,v 1.3 2023/11/11 18:35:35 robert Exp $

.include <bsd.own.mk>

LLDB_SRCS=	${.CURDIR}/../../../../../llvm/lldb

DEFS=	DynamicLoaderDarwinKernelProperties.inc \
	DynamicLoaderDarwinKernelPropertiesEnum.inc \
	JITLoaderGDBProperties.inc \
	JITLoaderGDBPropertiesEnum.inc \
	ObjectFilePECOFFProperties.inc \
	ObjectFilePECOFFPropertiesEnum.inc \
	PlatformMacOSXProperties.inc \
	PlatformMacOSXPropertiesEnum.inc \
	ProcessGDBRemoteProperties.inc \
	ProcessGDBRemotePropertiesEnum.inc \
	StructuredDataDarwinLogProperties.inc \
	StructuredDataDarwinLogPropertiesEnum.inc \
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
