# $OpenBSD: Makefile,v 1.9 2021/12/17 14:55:44 patrick Exp $

.include <bsd.own.mk>

LLVM_SRCS=	${.CURDIR}/../../../../../llvm/llvm

DEFS=	Attributes.inc \
	IntrinsicImpl.inc \
	IntrinsicEnums.inc \
	IntrinsicsAArch64.h \
	IntrinsicsAMDGPU.h \
	IntrinsicsARM.h \
	IntrinsicsBPF.h \
	IntrinsicsHexagon.h \
	IntrinsicsMips.h \
	IntrinsicsNVPTX.h \
	IntrinsicsPowerPC.h \
	IntrinsicsR600.h \
	IntrinsicsRISCV.h \
	IntrinsicsS390.h \
	IntrinsicsWebAssembly.h \
	IntrinsicsX86.h \
	IntrinsicsXCore.h \
	IntrinsicsVE.h

INCDIR=	/usr/include/llvm/IR

all: ${DEFS}

clean cleandir:
	rm -f ${DEFS}

Attributes.inc: ${LLVM_SRCS}/include/llvm/IR/Attributes.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-attrs \
		-I${LLVM_SRCS}/include/llvm/IR -I${LLVM_SRCS}/lib/Target \
		-I${LLVM_SRCS}/include \
		-o ${.TARGET} ${.ALLSRC}

IntrinsicImpl.inc: ${LLVM_SRCS}/include/llvm/IR/Intrinsics.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-intrinsic-impl \
		-I${LLVM_SRCS}/include/llvm/IR -I${LLVM_SRCS}/lib/Target \
		-I${LLVM_SRCS}/include \
		-o ${.TARGET} ${.ALLSRC}

IntrinsicEnums.inc: ${LLVM_SRCS}/include/llvm/IR/Intrinsics.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-intrinsic-enums \
		-I${LLVM_SRCS}/include/llvm/IR -I${LLVM_SRCS}/lib/Target \
		-I${LLVM_SRCS}/include \
		-o ${.TARGET} ${.ALLSRC}

IntrinsicsAArch64.h: ${LLVM_SRCS}/include/llvm/IR/Intrinsics.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-intrinsic-enums -intrinsic-prefix=aarch64 \
		-I${LLVM_SRCS}/include/llvm/IR -I${LLVM_SRCS}/lib/Target \
		-I${LLVM_SRCS}/include \
		-o ${.TARGET} ${.ALLSRC}

IntrinsicsAMDGPU.h: ${LLVM_SRCS}/include/llvm/IR/Intrinsics.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-intrinsic-enums -intrinsic-prefix=amdgcn \
		-I${LLVM_SRCS}/include/llvm/IR -I${LLVM_SRCS}/lib/Target \
		-I${LLVM_SRCS}/include \
		-o ${.TARGET} ${.ALLSRC}

IntrinsicsARM.h: ${LLVM_SRCS}/include/llvm/IR/Intrinsics.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-intrinsic-enums -intrinsic-prefix=arm \
		-I${LLVM_SRCS}/include/llvm/IR -I${LLVM_SRCS}/lib/Target \
		-I${LLVM_SRCS}/include \
		-o ${.TARGET} ${.ALLSRC}

IntrinsicsBPF.h: ${LLVM_SRCS}/include/llvm/IR/Intrinsics.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-intrinsic-enums -intrinsic-prefix=bpf \
		-I${LLVM_SRCS}/include/llvm/IR -I${LLVM_SRCS}/lib/Target \
		-I${LLVM_SRCS}/include \
		-o ${.TARGET} ${.ALLSRC}

IntrinsicsHexagon.h: ${LLVM_SRCS}/include/llvm/IR/Intrinsics.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-intrinsic-enums -intrinsic-prefix=hexagon \
		-I${LLVM_SRCS}/include/llvm/IR -I${LLVM_SRCS}/lib/Target \
		-I${LLVM_SRCS}/include \
		-o ${.TARGET} ${.ALLSRC}

IntrinsicsMips.h: ${LLVM_SRCS}/include/llvm/IR/Intrinsics.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-intrinsic-enums -intrinsic-prefix=mips \
		-I${LLVM_SRCS}/include/llvm/IR -I${LLVM_SRCS}/lib/Target \
		-I${LLVM_SRCS}/include \
		-o ${.TARGET} ${.ALLSRC}

IntrinsicsNVPTX.h: ${LLVM_SRCS}/include/llvm/IR/Intrinsics.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-intrinsic-enums -intrinsic-prefix=nvvm \
		-I${LLVM_SRCS}/include/llvm/IR -I${LLVM_SRCS}/lib/Target \
		-I${LLVM_SRCS}/include \
		-o ${.TARGET} ${.ALLSRC}

IntrinsicsPowerPC.h: ${LLVM_SRCS}/include/llvm/IR/Intrinsics.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-intrinsic-enums -intrinsic-prefix=ppc \
		-I${LLVM_SRCS}/include/llvm/IR -I${LLVM_SRCS}/lib/Target \
		-I${LLVM_SRCS}/include \
		-o ${.TARGET} ${.ALLSRC}

IntrinsicsR600.h: ${LLVM_SRCS}/include/llvm/IR/Intrinsics.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-intrinsic-enums -intrinsic-prefix=r600 \
		-I${LLVM_SRCS}/include/llvm/IR -I${LLVM_SRCS}/lib/Target \
		-I${LLVM_SRCS}/include \
		-o ${.TARGET} ${.ALLSRC}

IntrinsicsRISCV.h: ${LLVM_SRCS}/include/llvm/IR/Intrinsics.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-intrinsic-enums -intrinsic-prefix=riscv \
		-I${LLVM_SRCS}/include/llvm/IR -I${LLVM_SRCS}/lib/Target \
		-I${LLVM_SRCS}/include \
		-o ${.TARGET} ${.ALLSRC}

IntrinsicsS390.h: ${LLVM_SRCS}/include/llvm/IR/Intrinsics.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-intrinsic-enums -intrinsic-prefix=s390 \
		-I${LLVM_SRCS}/include/llvm/IR -I${LLVM_SRCS}/lib/Target \
		-I${LLVM_SRCS}/include \
		-o ${.TARGET} ${.ALLSRC}

IntrinsicsWebAssembly.h: ${LLVM_SRCS}/include/llvm/IR/Intrinsics.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-intrinsic-enums -intrinsic-prefix=wasm \
		-I${LLVM_SRCS}/include/llvm/IR -I${LLVM_SRCS}/lib/Target \
		-I${LLVM_SRCS}/include \
		-o ${.TARGET} ${.ALLSRC}

IntrinsicsX86.h: ${LLVM_SRCS}/include/llvm/IR/Intrinsics.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-intrinsic-enums -intrinsic-prefix=x86 \
		-I${LLVM_SRCS}/include/llvm/IR -I${LLVM_SRCS}/lib/Target \
		-I${LLVM_SRCS}/include \
		-o ${.TARGET} ${.ALLSRC}

IntrinsicsXCore.h: ${LLVM_SRCS}/include/llvm/IR/Intrinsics.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-intrinsic-enums -intrinsic-prefix=xcore \
		-I${LLVM_SRCS}/include/llvm/IR -I${LLVM_SRCS}/lib/Target \
		-I${LLVM_SRCS}/include \
		-o ${.TARGET} ${.ALLSRC}

IntrinsicsVE.h: ${LLVM_SRCS}/include/llvm/IR/Intrinsics.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-intrinsic-enums -intrinsic-prefix=ve \
		-I${LLVM_SRCS}/include/llvm/IR -I${LLVM_SRCS}/lib/Target \
		-I${LLVM_SRCS}/include \
		-o ${.TARGET} ${.ALLSRC}

install includes: ${DEFS}
	${INSTALL} -d -m 755 ${DESTDIR}${INCDIR}
	@cd ${.OBJDIR}; for i in $(DEFS); do \
	    j="cmp -s $$i ${DESTDIR}${INCDIR}/$$i || \
		${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} \
		-m 444 $$i ${DESTDIR}${INCDIR}"; \
	    echo $$j; \
	    eval "$$j"; \
	done

.include <bsd.obj.mk>
