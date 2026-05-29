# $OpenBSD:

LIB=	LLVMCGData
NOPROFILE=

CPPFLAGS+=-I${LLVM_SRC}/include/llvm/CGData

SRCS+=	CodeGenData.cpp \
	CodeGenDataReader.cpp \
	CodeGenDataWriter.cpp \
	OutlinedHashTree.cpp \
	OutlinedHashTreeRecord.cpp \
	StableFunctionMap.cpp \
	StableFunctionMapRecord.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/CGData
