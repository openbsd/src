# $OpenBSD: Makefile,v 1.6 2023/11/11 18:35:36 robert Exp $

.include <bsd.own.mk>

LLVM_SRCS=	${.CURDIR}/../../../../../llvm/llvm

HDRS=	MipsGenAsmMatcher.inc \
	MipsGenAsmWriter.inc \
	MipsGenCallingConv.inc \
	MipsGenDAGISel.inc \
	MipsGenDisassemblerTables.inc \
	MipsGenFastISel.inc \
	MipsGenGlobalISel.inc \
	MipsGenPostLegalizeGICombiner.inc \
	MipsGenInstrInfo.inc \
	MipsGenMCCodeEmitter.inc \
	MipsGenMCPseudoLowering.inc \
	MipsGenRegisterBank.inc \
	MipsGenRegisterInfo.inc \
	MipsGenSubtargetInfo.inc \
	MipsGenExegesis.inc

all: ${HDRS}

install:
	@# Nothing here so far ...

clean cleandir:
	rm -f ${HDRS}

MipsGenAsmMatcher.inc: ${LLVM_SRCS}/lib/Target/Mips/Mips.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-asm-matcher \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/Mips \
		-o ${.TARGET} ${.ALLSRC}

MipsGenAsmWriter.inc: ${LLVM_SRCS}/lib/Target/Mips/Mips.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-asm-writer \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/Mips \
		-o ${.TARGET} ${.ALLSRC}

MipsGenCallingConv.inc: ${LLVM_SRCS}/lib/Target/Mips/Mips.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-callingconv \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/Mips \
		-o ${.TARGET} ${.ALLSRC}

MipsGenDAGISel.inc: ${LLVM_SRCS}/lib/Target/Mips/Mips.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-dag-isel \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/Mips \
		-o ${.TARGET} ${.ALLSRC}

MipsGenDisassemblerTables.inc: ${LLVM_SRCS}/lib/Target/Mips/Mips.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-disassembler \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/Mips \
		-o ${.TARGET} ${.ALLSRC}

MipsGenFastISel.inc: ${LLVM_SRCS}/lib/Target/Mips/Mips.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-fast-isel \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/Mips \
		-o ${.TARGET} ${.ALLSRC}

MipsGenGlobalISel.inc: ${LLVM_SRCS}/lib/Target/Mips/Mips.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-global-isel \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/Mips \
		-o ${.TARGET} ${.ALLSRC}

MipsGenPostLegalizeGICombiner.inc: ${LLVM_SRCS}/lib/Target/Mips/Mips.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-global-isel-combiner \
		-combiners="MipsPostLegalizerCombinerHelper" \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/Mips \
		-o ${.TARGET} ${.ALLSRC}

MipsGenInstrInfo.inc: ${LLVM_SRCS}/lib/Target/Mips/Mips.td

MipsGenInstrInfo.inc: ${LLVM_SRCS}/lib/Target/Mips/Mips.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-instr-info \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/Mips \
		-o ${.TARGET} ${.ALLSRC}

MipsGenMCCodeEmitter.inc: ${LLVM_SRCS}/lib/Target/Mips/Mips.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-emitter \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/Mips \
		-o ${.TARGET} ${.ALLSRC}

MipsGenMCPseudoLowering.inc: ${LLVM_SRCS}/lib/Target/Mips/Mips.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-pseudo-lowering \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/Mips \
		-o ${.TARGET} ${.ALLSRC}

MipsGenRegisterBank.inc: ${LLVM_SRCS}/lib/Target/Mips/Mips.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-register-bank \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/Mips \
		-o ${.TARGET} ${.ALLSRC}

MipsGenRegisterInfo.inc: ${LLVM_SRCS}/lib/Target/Mips/Mips.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-register-info \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/Mips \
		-o ${.TARGET} ${.ALLSRC}

MipsGenSubtargetInfo.inc: ${LLVM_SRCS}/lib/Target/Mips/Mips.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-subtarget \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/Mips \
		-o ${.TARGET} ${.ALLSRC}

MipsGenExegesis.inc: ${LLVM_SRCS}/lib/Target/Mips/Mips.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-exegesis \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/Mips \
		-o ${.TARGET} ${.ALLSRC}

.include <bsd.obj.mk>
