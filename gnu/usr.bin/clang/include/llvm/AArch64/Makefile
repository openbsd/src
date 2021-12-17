# $OpenBSD: Makefile,v 1.7 2021/12/17 14:55:44 patrick Exp $

.include <bsd.own.mk>

LLVM_SRCS=	${.CURDIR}/../../../../../llvm/llvm

HDRS=	AArch64GenAsmMatcher.inc \
	AArch64GenAsmWriter.inc \
	AArch64GenAsmWriter1.inc \
	AArch64GenCallingConv.inc \
	AArch64GenDAGISel.inc \
	AArch64GenDisassemblerTables.inc \
	AArch64GenFastISel.inc \
	AArch64GenGlobalISel.inc \
	AArch64GenO0PreLegalizeGICombiner.inc \
	AArch64GenPreLegalizeGICombiner.inc \
	AArch64GenPostLegalizeGICombiner.inc \
	AArch64GenPostLegalizeGILowering.inc \
	AArch64GenInstrInfo.inc \
	AArch64GenMCCodeEmitter.inc \
	AArch64GenMCPseudoLowering.inc \
	AArch64GenRegisterBank.inc \
	AArch64GenRegisterInfo.inc \
	AArch64GenSubtargetInfo.inc \
	AArch64GenSystemOperands.inc \
	AArch64GenExegesis.inc

all: ${HDRS}

install:
	@# Nothing here so far ...

clean cleandir:
	rm -f ${HDRS}

AArch64GenAsmMatcher.inc: ${LLVM_SRCS}/lib/Target/AArch64/AArch64.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-asm-matcher \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/AArch64 \
		-o ${.TARGET} ${.ALLSRC}

AArch64GenAsmWriter.inc: ${LLVM_SRCS}/lib/Target/AArch64/AArch64.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-asm-writer \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/AArch64 \
		-o ${.TARGET} ${.ALLSRC}

AArch64GenAsmWriter1.inc: ${LLVM_SRCS}/lib/Target/AArch64/AArch64.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-asm-writer \
		-asmwriternum=1 -I${LLVM_SRCS}/include \
		-I${LLVM_SRCS}/lib/Target/AArch64 -o ${.TARGET} ${.ALLSRC}

AArch64GenCallingConv.inc: ${LLVM_SRCS}/lib/Target/AArch64/AArch64.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-callingconv \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/AArch64 \
		-o ${.TARGET} ${.ALLSRC}

AArch64GenDAGISel.inc: ${LLVM_SRCS}/lib/Target/AArch64/AArch64.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-dag-isel \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/AArch64 \
		-o ${.TARGET} ${.ALLSRC}

AArch64GenDisassemblerTables.inc: ${LLVM_SRCS}/lib/Target/AArch64/AArch64.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-disassembler \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/AArch64 \
		-o ${.TARGET} ${.ALLSRC}

AArch64GenFastISel.inc: ${LLVM_SRCS}/lib/Target/AArch64/AArch64.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-fast-isel \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/AArch64 \
		-o ${.TARGET} ${.ALLSRC}

AArch64GenGlobalISel.inc: ${LLVM_SRCS}/lib/Target/AArch64/AArch64.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-global-isel \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/AArch64 \
		-o ${.TARGET} ${.ALLSRC}

AArch64GenO0PreLegalizeGICombiner.inc: ${LLVM_SRCS}/lib/Target/AArch64/AArch64.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-global-isel-combiner \
		-combiners="AArch64O0PreLegalizerCombinerHelper" \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/AArch64 \
		-o ${.TARGET} ${.ALLSRC}

AArch64GenPreLegalizeGICombiner.inc: ${LLVM_SRCS}/lib/Target/AArch64/AArch64.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-global-isel-combiner \
		-combiners="AArch64PreLegalizerCombinerHelper" \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/AArch64 \
		-o ${.TARGET} ${.ALLSRC}

AArch64GenPostLegalizeGICombiner.inc: ${LLVM_SRCS}/lib/Target/AArch64/AArch64.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-global-isel-combiner \
		-combiners="AArch64PostLegalizerCombinerHelper" \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/AArch64 \
		-o ${.TARGET} ${.ALLSRC}

AArch64GenPostLegalizeGILowering.inc: ${LLVM_SRCS}/lib/Target/AArch64/AArch64.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-global-isel-combiner \
		-combiners="AArch64PostLegalizerLoweringHelper" \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/AArch64 \
		-o ${.TARGET} ${.ALLSRC}

AArch64GenInstrInfo.inc: ${LLVM_SRCS}/lib/Target/AArch64/AArch64.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-instr-info \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/AArch64 \
		-o ${.TARGET} ${.ALLSRC}

AArch64GenMCCodeEmitter.inc: ${LLVM_SRCS}/lib/Target/AArch64/AArch64.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-emitter \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/AArch64 \
		-o ${.TARGET} ${.ALLSRC}

AArch64GenMCPseudoLowering.inc: ${LLVM_SRCS}/lib/Target/AArch64/AArch64.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-pseudo-lowering \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/AArch64 \
		-o ${.TARGET} ${.ALLSRC}

AArch64GenRegisterBank.inc: ${LLVM_SRCS}/lib/Target/AArch64/AArch64.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-register-bank \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/AArch64 \
		-o ${.TARGET} ${.ALLSRC}

AArch64GenRegisterInfo.inc: ${LLVM_SRCS}/lib/Target/AArch64/AArch64.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-register-info \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/AArch64 \
		-o ${.TARGET} ${.ALLSRC}

AArch64GenSubtargetInfo.inc: ${LLVM_SRCS}/lib/Target/AArch64/AArch64.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-subtarget \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/AArch64 \
		-o ${.TARGET} ${.ALLSRC}

AArch64GenSystemOperands.inc: ${LLVM_SRCS}/lib/Target/AArch64/AArch64.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-searchable-tables \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/AArch64 \
		-o ${.TARGET} ${.ALLSRC}

AArch64GenExegesis.inc: ${LLVM_SRCS}/lib/Target/AArch64/AArch64.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-exegesis \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/AArch64 \
		-o ${.TARGET} ${.ALLSRC}

.include <bsd.obj.mk>
