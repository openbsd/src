# $OpenBSD: Makefile,v 1.2 2021/12/17 14:55:44 patrick Exp $

.include <bsd.own.mk>

LLVM_SRCS=	${.CURDIR}/../../../../../llvm/llvm

HDRS=	RISCVGenAsmMatcher.inc RISCVGenAsmWriter.inc \
	RISCVGenCallingConv.inc RISCVGenDAGISel.inc \
	RISCVGenDisassemblerTables.inc \
	RISCVGenInstrInfo.inc RISCVGenRegisterInfo.inc \
	RISCVGenSubtargetInfo.inc \
	RISCVGenMCCodeEmitter.inc RISCVGenMCPseudoLowering.inc \
	RISCVGenSearchableTables.inc RISCVGenRegisterBank.inc \
	RISCVGenGlobalISel.inc RISCVGenCompressInstEmitter.inc

# RISCVGenGICombiner.inc  RISCVGenFastISel.inc 

all: ${HDRS}

install:
	@# Nothing here so far ...

clean cleandir:
	rm -f ${HDRS}

RISCVGenRegisterInfo.inc: ${LLVM_SRCS}/lib/Target/RISCV/RISCV.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-register-info \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/RISCV \
		-o ${.TARGET} ${.ALLSRC}

RISCVGenDisassemblerTables.inc: ${LLVM_SRCS}/lib/Target/RISCV/RISCV.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-disassembler \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/RISCV \
		-o ${.TARGET} ${.ALLSRC}

RISCVGenInstrInfo.inc: ${LLVM_SRCS}/lib/Target/RISCV/RISCV.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-instr-info \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/RISCV \
		-o ${.TARGET} ${.ALLSRC}

RISCVGenAsmWriter.inc: ${LLVM_SRCS}/lib/Target/RISCV/RISCV.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-asm-writer \
		-I${LLVM_SRCS}/include \
		-I${LLVM_SRCS}/lib/Target/RISCV -o ${.TARGET} ${.ALLSRC}

RISCVGenAsmMatcher.inc: ${LLVM_SRCS}/lib/Target/RISCV/RISCV.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-asm-matcher \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/RISCV \
		-o ${.TARGET} ${.ALLSRC}

RISCVGenDAGISel.inc: ${LLVM_SRCS}/lib/Target/RISCV/RISCV.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-dag-isel \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/RISCV \
		-o ${.TARGET} ${.ALLSRC}

RISCVGenFastISel.inc: ${LLVM_SRCS}/lib/Target/RISCV/RISCV.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-fast-isel \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/RISCV \
		-o ${.TARGET} ${.ALLSRC}

RISCVGenCallingConv.inc: ${LLVM_SRCS}/lib/Target/RISCV/RISCV.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-callingconv \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/RISCV \
		-o ${.TARGET} ${.ALLSRC}

RISCVGenSubtargetInfo.inc: ${LLVM_SRCS}/lib/Target/RISCV/RISCV.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-subtarget \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/RISCV \
		-o ${.TARGET} ${.ALLSRC}

RISCVGenMCCodeEmitter.inc: ${LLVM_SRCS}/lib/Target/RISCV/RISCV.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-emitter \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/RISCV \
		-o ${.TARGET} ${.ALLSRC}

RISCVGenMCPseudoLowering.inc: ${LLVM_SRCS}/lib/Target/RISCV/RISCV.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-pseudo-lowering \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/RISCV \
		-o ${.TARGET} ${.ALLSRC}

RISCVGenSearchableTables.inc: ${LLVM_SRCS}/lib/Target/RISCV/RISCV.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-searchable-tables \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/RISCV \
		-o ${.TARGET} ${.ALLSRC}

RISCVGenRegisterBank.inc: ${LLVM_SRCS}/lib/Target/RISCV/RISCV.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-register-bank \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/RISCV \
		-o ${.TARGET} ${.ALLSRC}

RISCVGenGlobalISel.inc: ${LLVM_SRCS}/lib/Target/RISCV/RISCV.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-global-isel \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/RISCV \
		-o ${.TARGET} ${.ALLSRC}

RISCVGenCompressInstEmitter.inc: ${LLVM_SRCS}/lib/Target/RISCV/RISCV.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen --gen-compress-inst-emitter \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/RISCV \
		-o ${.TARGET} ${.ALLSRC}

RISCVGenGICombiner.inc: ${LLVM_SRCS}/lib/Target/RISCV/RISCV.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-global-isel-combiner \
		-combiners="RISCVPreLegalizerCombinerHelper" \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/RISCV \
		-o ${.TARGET} ${.ALLSRC}

.include <bsd.obj.mk>
