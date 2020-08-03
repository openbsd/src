# $OpenBSD: Makefile,v 1.1 2020/08/03 14:45:23 patrick Exp $

.include <bsd.own.mk>

LLDB_SRCS=	${.CURDIR}/../../../../../llvm/lldb

DEFS=	DynamicLoaderDarwinKernelProperties.inc
DEFS+=	DynamicLoaderDarwinKernelPropertiesEnum.inc
DEFS+=	JITLoaderGDBProperties.inc
DEFS+=	JITLoaderGDBPropertiesEnum.inc
DEFS+=	ProcessGDBRemoteProperties.inc
DEFS+=	ProcessGDBRemotePropertiesEnum.inc
DEFS+=	StructuredDataDarwinLogProperties.inc
DEFS+=	StructuredDataDarwinLogPropertiesEnum.inc
DEFS+=	SymbolFileDWARFProperties.inc
DEFS+=	SymbolFileDWARFPropertiesEnum.inc

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
