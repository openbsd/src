# $OpenBSD: Makefile,v 1.25 2021/12/17 14:55:44 patrick Exp $

.include <bsd.own.mk>

LLVM_V=			13.0.0
CLANG_INTR_INCDIR=	/usr/lib/clang/${LLVM_V}/include

TBLGEN= ${.OBJDIR}/../../../clang-tblgen/clang-tblgen
CLANG_INC=${.CURDIR}/../../../../../llvm/clang/include

.PATH:	${.CURDIR}/../../../../../llvm/clang/lib/Headers

GEN=
HEADERS=	stdalign.h \
		stdatomic.h \
		stdnoreturn.h \
		tgmath.h \
		unwind.h \
		varargs.h \
		module.modulemap \

.if ${MACHINE_ARCH} == "aarch64" || ${MACHINE_ARCH} == "arm"
GEN+=		arm_neon.h
GEN+=		arm_fp16.h
GEN+=		arm_sve.h
GEN+=		arm_bf16.h
GEN+=		arm_mve.h
GEN+=		arm_cde.h
HEADERS+=	arm_acle.h arm_cmse.h ${GEN}
.elif ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "i386"
HEADERS+=	adxintrin.h \
		ammintrin.h \
		amxintrin.h \
		avx2intrin.h \
		avx512bf16intrin.h \
		avx512bwintrin.h \
		avx512bitalgintrin.h \
		avx512vlbitalgintrin.h \
		avx512cdintrin.h \
		avx512vpopcntdqintrin.h \
		avx512dqintrin.h \
		avx512erintrin.h \
		avx512fintrin.h \
		avx512ifmaintrin.h \
		avx512ifmavlintrin.h \
		avx512pfintrin.h \
		avx512vbmiintrin.h \
		avx512vbmivlintrin.h \
		avx512vbmi2intrin.h \
		avx512vlvbmi2intrin.h \
		avx512vlbf16intrin.h \
		avx512vlbwintrin.h \
		avx512vlcdintrin.h \
		avx512vldqintrin.h \
		avx512vlintrin.h \
		avx512vp2intersectintrin.h \
		avx512vlvp2intersectintrin.h \
		avx512vpopcntdqvlintrin.h \
		avx512vnniintrin.h \
		avx512vlvnniintrin.h \
		avxintrin.h \
		avxvnniintrin.h \
		bmi2intrin.h \
		bmiintrin.h \
		cetintrin.h \
		cet.h \
		cldemoteintrin.h \
		clzerointrin.h \
		cpuid.h \
		clflushoptintrin.h \
		clwbintrin.h \
		emmintrin.h \
		enqcmdintrin.h \
		f16cintrin.h \
		fma4intrin.h \
		fmaintrin.h \
		fxsrintrin.h \
		gfniintrin.h \
		hresetintrin.h \
		ia32intrin.h \
		immintrin.h \
		invpcidintrin.h \
		keylockerintrin.h \
		lwpintrin.h \
		lzcntintrin.h \
		mm3dnow.h \
		mmintrin.h \
		mm_malloc.h \
		movdirintrin.h \
		mwaitxintrin.h \
		nmmintrin.h \
		pconfigintrin.h \
		pkuintrin.h \
		pmmintrin.h \
		popcntintrin.h \
		prfchwintrin.h \
		ptwriteintrin.h \
		rdseedintrin.h \
		rtmintrin.h \
		serializeintrin.h \
		sgxintrin.h \
		shaintrin.h \
		smmintrin.h \
		tbmintrin.h \
		tmmintrin.h \
		tsxldtrkintrin.h \
		uintrintrin.h \
		vaesintrin.h \
		vpclmulqdqintrin.h \
		waitpkgintrin.h \
		wasm_simd128.h \
		wbnoinvdintrin.h \
		wmmintrin.h \
		__wmmintrin_aes.h \
		__wmmintrin_pclmul.h \
		x86gprintrin.h \
		x86intrin.h \
		xmmintrin.h \
		xopintrin.h \
		xsavecintrin.h \
		xsaveintrin.h \
		xsaveoptintrin.h \
		xsavesintrin.h \
		xtestintrin.h
.elif ${MACHINE_ARCH} == "powerpc" || ${MACHINE_ARCH} == "powerpc64"
HEADERS+=	altivec.h \
		htmintrin.h \
		htmxlintrin.h
.elif ${MACHINE_ARCH} == "riscv64"
GEN+=		riscv_vector.h
.endif

all:	${HEADERS}

clean cleandir:
	rm -f ${GEN}

install includes: ${HEADERS}
.for dir in /usr/lib/clang /usr/lib/clang/${LLVM_V} ${CLANG_INTR_INCDIR}
	${INSTALL} -d -o ${BINOWN} -g ${BINGRP} -m ${DIRMODE} \
	    ${DESTDIR}${dir}
.endfor
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.ALLSRC} ${DESTDIR}${CLANG_INTR_INCDIR}

arm_neon.h: ${CLANG_INC}/clang/Basic/arm_neon.td
	${TBLGEN} -gen-arm-neon -I${CLANG_INC}/clang/Basic \
	    -o ${.TARGET} ${.ALLSRC}

arm_fp16.h: ${CLANG_INC}/clang/Basic/arm_fp16.td
	${TBLGEN} -gen-arm-fp16 -I${CLANG_INC}/clang/Basic \
	    -o ${.TARGET} ${.ALLSRC}

arm_sve.h: ${CLANG_INC}/clang/Basic/arm_sve.td
	${TBLGEN} -gen-arm-sve-header -I${CLANG_INC}/clang/Basic \
	    -o ${.TARGET} ${.ALLSRC}

arm_bf16.h: ${CLANG_INC}/clang/Basic/arm_bf16.td
	${TBLGEN} -gen-arm-bf16 -I${CLANG_INC}/clang/Basic \
	    -o ${.TARGET} ${.ALLSRC}

arm_mve.h: ${CLANG_INC}/clang/Basic/arm_mve.td
	${TBLGEN} -gen-arm-mve-header -I${CLANG_INC}/clang/Basic \
	    -o ${.TARGET} ${.ALLSRC}

arm_cde.h: ${CLANG_INC}/clang/Basic/arm_cde.td
	${TBLGEN} -gen-arm-cde-header -I${CLANG_INC}/clang/Basic \
	    -o ${.TARGET} ${.ALLSRC}

riscv_vector.h: ${CLANG_INC}/clang/Basic/riscv_vector.td
	${TBLGEN} -gen-riscv-vector-header -I${CLANG_INC}/clang/Basic \
	    -o ${.TARGET} ${.ALLSRC}

.include <bsd.obj.mk>
