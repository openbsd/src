# $OpenBSD: Makefile,v 1.6 2019/06/23 22:07:38 patrick Exp $

.include <bsd.own.mk>

LLVM_SRCS=	${.CURDIR}/../../../../../llvm

HDRS=	X86GenAsmMatcher.inc X86GenAsmWriter.inc X86GenAsmWriter1.inc \
	X86GenCallingConv.inc X86GenDAGISel.inc X86GenDisassemblerTables.inc \
	X86GenEVEX2VEXTables.inc X86GenExegesis.inc X86GenFastISel.inc \
	X86GenInstrInfo.inc X86GenRegisterInfo.inc X86GenSubtargetInfo.inc \
	X86GenRegisterBank.inc X86GenGlobalISel.inc

all: ${HDRS}

install:
	@# Nothing here so far ...

clean cleandir:
	rm -f ${HDRS}

X86GenRegisterInfo.inc: ${LLVM_SRCS}/lib/Target/X86/X86.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-register-info \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/X86 \
		-o ${.TARGET} ${.ALLSRC}

X86GenDisassemblerTables.inc: ${LLVM_SRCS}/lib/Target/X86/X86.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-disassembler \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/X86 \
		-o ${.TARGET} ${.ALLSRC}

X86GenInstrInfo.inc: ${LLVM_SRCS}/lib/Target/X86/X86.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-instr-info \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/X86 \
		-o ${.TARGET} ${.ALLSRC}

X86GenAsmWriter.inc: ${LLVM_SRCS}/lib/Target/X86/X86.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-asm-writer \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/X86 \
		-o ${.TARGET} ${.ALLSRC}

X86GenAsmWriter1.inc: ${LLVM_SRCS}/lib/Target/X86/X86.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-asm-writer -asmwriternum=1 \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/X86 \
		-o ${.TARGET} ${.ALLSRC}

X86GenAsmMatcher.inc: ${LLVM_SRCS}/lib/Target/X86/X86.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-asm-matcher \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/X86 \
		-o ${.TARGET} ${.ALLSRC}

X86GenDAGISel.inc: ${LLVM_SRCS}/lib/Target/X86/X86.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-dag-isel \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/X86 \
		-o ${.TARGET} ${.ALLSRC}

X86GenFastISel.inc: ${LLVM_SRCS}/lib/Target/X86/X86.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-fast-isel \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/X86 \
		-o ${.TARGET} ${.ALLSRC}

X86GenCallingConv.inc: ${LLVM_SRCS}/lib/Target/X86/X86.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-callingconv \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/X86 \
		-o ${.TARGET} ${.ALLSRC}

X86GenSubtargetInfo.inc: ${LLVM_SRCS}/lib/Target/X86/X86.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-subtarget \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/X86 \
		-o ${.TARGET} ${.ALLSRC}

X86GenEVEX2VEXTables.inc: ${LLVM_SRCS}/lib/Target/X86/X86.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-x86-EVEX2VEX-tables \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/X86 \
		-o ${.TARGET} ${.ALLSRC}

X86GenExegesis.inc: ${LLVM_SRCS}/lib/Target/X86/X86.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-exegesis \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/X86 \
		-o ${.TARGET} ${.ALLSRC}

X86GenRegisterBank.inc: ${LLVM_SRCS}/lib/Target/X86/X86.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-register-bank \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/X86 \
		-o ${.TARGET} ${.ALLSRC}

X86GenGlobalISel.inc: ${LLVM_SRCS}/lib/Target/X86/X86.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-global-isel \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/X86 \
		-o ${.TARGET} ${.ALLSRC}

.include <bsd.obj.mk>
