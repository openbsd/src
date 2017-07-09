# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:34 espie Exp $

.include <bsd.own.mk>

LLVM_SRCS=	${.CURDIR}/../../../../../llvm

HDRS=	PPCGenAsmMatcher.inc PPCGenAsmWriter.inc \
	PPCGenCallingConv.inc \
	PPCGenDAGISel.inc PPCGenDisassemblerTables.inc \
	PPCGenFastISel.inc PPCGenInstrInfo.inc \
	PPCGenRegisterInfo.inc PPCGenSubtargetInfo.inc \
	PPCGenMCCodeEmitter.inc

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

.include <bsd.obj.mk>
