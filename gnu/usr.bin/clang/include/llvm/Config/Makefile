# $OpenBSD: Makefile,v 1.3 2017/07/09 15:28:34 espie Exp $

.include <bsd.own.mk>

.include "../../../Makefile.arch"

DEFS=	AsmParsers.def AsmPrinters.def Disassemblers.def Targets.def

all: ${DEFS}

install:
	@# Nothing here so far ...

clean cleandir:
	rm -f ${DEFS}

AsmParsers.def:
	printf "LLVM_ASM_PARSER(${LLVM_ARCH})\n#undef LLVM_ASM_PARSER\n" >$@
AsmPrinters.def:
	printf "LLVM_ASM_PRINTER(${LLVM_ARCH})\n#undef LLVM_ASM_PRINTER\n" >$@
Disassemblers.def:
	printf "LLVM_DISASSEMBLER(${LLVM_ARCH})\n#undef LLVM_DISASSEMBLER\n" >$@
Targets.def:
	printf "LLVM_TARGET(${LLVM_ARCH})\n#undef LLVM_TARGET\n" >$@

.include <bsd.obj.mk>
