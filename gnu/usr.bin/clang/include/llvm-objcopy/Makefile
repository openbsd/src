# $OpenBSD: Makefile,v 1.2 2021/12/17 14:55:44 patrick Exp $

.include <bsd.own.mk>

TBLGEN=	${.OBJDIR}/../../llvm-tblgen/llvm-tblgen
OBJCOPY_INC=${.CURDIR}/../../../../llvm/llvm/tools/llvm-objcopy

GEN=	InstallNameToolOpts.inc \
	ObjcopyOpts.inc \
	StripOpts.inc \
	BitcodeStripOpts.inc

all: ${GEN}

install:
	@# Nothing here so far ...

clean cleandir:
	rm -f ${GEN}

InstallNameToolOpts.inc: ${OBJCOPY_INC}/InstallNameToolOpts.td
	${TBLGEN} -I${.CURDIR}/../../../../llvm/llvm/include \
		-I${.CURDIR}/../../../../llvm/llvm/tools/llvm-objcopy \
		-gen-opt-parser-defs -o ${.TARGET} ${.ALLSRC}

ObjcopyOpts.inc: ${OBJCOPY_INC}/ObjcopyOpts.td
	${TBLGEN} -I${.CURDIR}/../../../../llvm/llvm/include \
		-I${.CURDIR}/../../../../llvm/llvm/tools/llvm-objcopy \
		-gen-opt-parser-defs -o ${.TARGET} ${.ALLSRC}

StripOpts.inc: ${OBJCOPY_INC}/StripOpts.td
	${TBLGEN} -I${.CURDIR}/../../../../llvm/llvm/include \
		-I${.CURDIR}/../../../../llvm/llvm/tools/llvm-objcopy \
		-gen-opt-parser-defs -o ${.TARGET} ${.ALLSRC}

BitcodeStripOpts.inc: ${OBJCOPY_INC}/BitcodeStripOpts.td
	${TBLGEN} -I${.CURDIR}/../../../../llvm/llvm/include \
		-I${.CURDIR}/../../../../llvm/llvm/tools/llvm-objcopy \
		-gen-opt-parser-defs -o ${.TARGET} ${.ALLSRC}

.include <bsd.obj.mk>
