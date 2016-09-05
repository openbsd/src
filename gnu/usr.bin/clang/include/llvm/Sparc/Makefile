# $OpenBSD: Makefile,v 1.1 2016/09/05 10:56:48 pascal Exp $

.include <bsd.own.mk>

LLVM_SRCS=	${.CURDIR}/../../../../../llvm

HDRS=	SparcGenAsmMatcher.inc SparcGenAsmWriter.inc \
	SparcGenCodeEmitter.inc SparcGenCallingConv.inc \
	SparcGenDAGISel.inc SparcGenDisassemblerTables.inc \
	SparcGenFastISel.inc SparcGenInstrInfo.inc \
	SparcGenRegisterInfo.inc SparcGenSubtargetInfo.inc \
	SparcGenMCCodeEmitter.inc

all: ${HDRS}

install:
	# Nothing here so far ...

depend:
	# Nothing here so far ...

clean:
	rm -f ${HDRS}

SparcGenAsmMatcher.inc: ${LLVM_SRCS}/lib/Target/Sparc/Sparc.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-asm-matcher \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/Sparc \
		-o ${.TARGET} ${.ALLSRC}

SparcGenAsmWriter.inc: ${LLVM_SRCS}/lib/Target/Sparc/Sparc.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-asm-writer \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/Sparc \
		-o ${.TARGET} ${.ALLSRC}

SparcGenCallingConv.inc: ${LLVM_SRCS}/lib/Target/Sparc/Sparc.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-callingconv \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/Sparc \
		-o ${.TARGET} ${.ALLSRC}

SparcGenCodeEmitter.inc: ${LLVM_SRCS}/lib/Target/Sparc/Sparc.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-emitter \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/Sparc \
		-o ${.TARGET} ${.ALLSRC}

SparcGenDAGISel.inc: ${LLVM_SRCS}/lib/Target/Sparc/Sparc.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-dag-isel \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/Sparc \
		-o ${.TARGET} ${.ALLSRC}

SparcGenDisassemblerTables.inc: ${LLVM_SRCS}/lib/Target/Sparc/Sparc.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-disassembler \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/Sparc \
		-o ${.TARGET} ${.ALLSRC}

SparcGenFastISel.inc: ${LLVM_SRCS}/lib/Target/Sparc/Sparc.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-fast-isel \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/Sparc \
		-o ${.TARGET} ${.ALLSRC}

SparcGenInstrInfo.inc: ${LLVM_SRCS}/lib/Target/Sparc/Sparc.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-instr-info \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/Sparc \
		-o ${.TARGET} ${.ALLSRC}

SparcGenMCCodeEmitter.inc: ${LLVM_SRCS}/lib/Target/Sparc/Sparc.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-emitter \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/Sparc \
		-o ${.TARGET} ${.ALLSRC}

SparcGenRegisterInfo.inc: ${LLVM_SRCS}/lib/Target/Sparc/Sparc.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-register-info \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/Sparc \
		-o ${.TARGET} ${.ALLSRC}

SparcGenSubtargetInfo.inc: ${LLVM_SRCS}/lib/Target/Sparc/Sparc.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-subtarget \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/Sparc \
		-o ${.TARGET} ${.ALLSRC}

.include <bsd.obj.mk>
