# $OpenBSD: Makefile,v 1.1 2026/05/29 11:18:31 robert Exp $

LIB=	LLVMObjectYAML
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/ObjectYAML

SRCS+=	ArchiveEmitter.cpp \
	ArchiveYAML.cpp \
	CodeViewYAMLDebugSections.cpp \
	CodeViewYAMLSymbols.cpp \
	CodeViewYAMLTypeHashing.cpp \
	CodeViewYAMLTypes.cpp \
	COFFEmitter.cpp \
	COFFYAML.cpp \
	DWARFEmitter.cpp \
	DWARFYAML.cpp \
	DXContainerEmitter.cpp \
	DXContainerYAML.cpp \
	ELFEmitter.cpp \
	ELFYAML.cpp \
	GOFFEmitter.cpp \
	GOFFYAML.cpp \
	MachOEmitter.cpp \
	MachOYAML.cpp \
	ObjectYAML.cpp \
	MinidumpEmitter.cpp \
	MinidumpYAML.cpp \
	OffloadEmitter.cpp \
	OffloadYAML.cpp \
	WasmEmitter.cpp \
	WasmYAML.cpp \
	XCOFFEmitter.cpp \
	XCOFFYAML.cpp \
	YAML.cpp \
	yaml2obj.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ObjectYAML
