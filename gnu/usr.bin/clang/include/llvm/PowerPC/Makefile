# $OpenBSD: Makefile,v 1.5 2021/12/17 14:55:44 patrick Exp $

.include <bsd.own.mk>

LLVM_SRCS=	${.CURDIR}/../../../../../llvm/llvm

HDRS=	PPCGenAsmMatcher.inc \
	PPCGenAsmWriter.inc \
	PPCGenCallingConv.inc \
	PPCGenDAGISel.inc \
	PPCGenDisassemblerTables.inc \
	PPCGenFastISel.inc \
	PPCGenInstrInfo.inc \
	PPCGenMCCodeEmitter.inc \
	PPCGenRegisterInfo.inc \
	PPCGenSubtargetInfo.inc \
	PPCGenExegesis.inc \
	PPCGenRegisterBank.inc \
	PPCGenGlobalISel.inc

all: ${HDRS}

install:
	@# Nothing here so far ...

clean cleandir:
	rm -f ${HDRS}

PPCGenAsmMatcher.inc: ${LLVM_SRCS}/lib/Target/PowerPC/PPC.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-asm-matcher \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/PowerPC \
		-o ${.TARGET} ${.ALLSRC}

PPCGenAsmWriter.inc: ${LLVM_SRCS}/lib/Target/PowerPC/PPC.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-asm-writer \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/PowerPC \
		-o ${.TARGET} ${.ALLSRC}

PPCGenCallingConv.inc: ${LLVM_SRCS}/lib/Target/PowerPC/PPC.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-callingconv \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/PowerPC \
		-o ${.TARGET} ${.ALLSRC}

PPCGenDAGISel.inc: ${LLVM_SRCS}/lib/Target/PowerPC/PPC.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-dag-isel \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/PowerPC \
		-o ${.TARGET} ${.ALLSRC}

PPCGenDisassemblerTables.inc: ${LLVM_SRCS}/lib/Target/PowerPC/PPC.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-disassembler \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/PowerPC \
		-o ${.TARGET} ${.ALLSRC}

PPCGenFastISel.inc: ${LLVM_SRCS}/lib/Target/PowerPC/PPC.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-fast-isel \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/PowerPC \
		-o ${.TARGET} ${.ALLSRC}

PPCGenInstrInfo.inc: ${LLVM_SRCS}/lib/Target/PowerPC/PPC.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-instr-info \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/PowerPC \
		-o ${.TARGET} ${.ALLSRC}

PPCGenMCCodeEmitter.inc: ${LLVM_SRCS}/lib/Target/PowerPC/PPC.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-emitter \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/PowerPC \
		-o ${.TARGET} ${.ALLSRC}

PPCGenRegisterInfo.inc: ${LLVM_SRCS}/lib/Target/PowerPC/PPC.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-register-info \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/PowerPC \
		-o ${.TARGET} ${.ALLSRC}

PPCGenSubtargetInfo.inc: ${LLVM_SRCS}/lib/Target/PowerPC/PPC.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-subtarget \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/PowerPC \
		-o ${.TARGET} ${.ALLSRC}

PPCGenExegesis.inc: ${LLVM_SRCS}/lib/Target/PowerPC/PPC.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-exegesis \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/PowerPC \
		-o ${.TARGET} ${.ALLSRC}

PPCGenRegisterBank.inc: ${LLVM_SRCS}/lib/Target/PowerPC/PPC.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-register-bank \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/PowerPC \
		-o ${.TARGET} ${.ALLSRC}

PPCGenGlobalISel.inc: ${LLVM_SRCS}/lib/Target/PowerPC/PPC.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-global-isel \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/PowerPC \
		-o ${.TARGET} ${.ALLSRC}

.include <bsd.obj.mk>
