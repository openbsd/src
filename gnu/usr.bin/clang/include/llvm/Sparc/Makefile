# $OpenBSD: Makefile,v 1.6 2025/06/11 17:46:32 robert Exp $

.include <bsd.own.mk>

LLVM_SRCS=	${.CURDIR}/../../../../../llvm/llvm

HDRS=	SparcGenAsmMatcher.inc \
	SparcGenAsmWriter.inc \
	SparcGenCallingConv.inc \
	SparcGenDAGISel.inc \
	SparcGenDisassemblerTables.inc \
	SparcGenInstrInfo.inc \
	SparcGenMCCodeEmitter.inc \
	SparcGenRegisterInfo.inc \
	SparcGenSearchableTables.inc \
	SparcGenSubtargetInfo.inc

all: ${HDRS}

install:
	@# Nothing here so far ...

clean cleandir:
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

SparcGenDAGISel.inc: ${LLVM_SRCS}/lib/Target/Sparc/Sparc.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-dag-isel \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/Sparc \
		-o ${.TARGET} ${.ALLSRC}

SparcGenDisassemblerTables.inc: ${LLVM_SRCS}/lib/Target/Sparc/Sparc.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-disassembler \
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

SparcGenSearchableTables.inc: ${LLVM_SRCS}/lib/Target/Sparc/Sparc.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-searchable-tables \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/Sparc \
		-o ${.TARGET} ${.ALLSRC}

SparcGenSubtargetInfo.inc: ${LLVM_SRCS}/lib/Target/Sparc/Sparc.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-subtarget \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/Sparc \
		-o ${.TARGET} ${.ALLSRC}

.include <bsd.obj.mk>
