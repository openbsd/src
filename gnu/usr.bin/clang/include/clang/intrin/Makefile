# $OpenBSD: Makefile,v 1.1 2016/09/05 10:56:48 pascal Exp $

.include <bsd.own.mk>

LLVM_V=			3.8.1
CLANG_INTR_INCDIR=	/usr/lib/clang/${LLVM_V}/include

.PATH:	${.CURDIR}/../../../../../llvm/tools/clang/lib/Headers

HEADERS=adxintrin.h \
	altivec.h \
	ammintrin.h \
	arm_acle.h \
	avx2intrin.h \
	avx512bwintrin.h \
	avx512cdintrin.h \
	avx512erintrin.h \
	avx512fintrin.h \
	avx512vlbwintrin.h \
	avx512vlintrin.h \
	avx512dqintrin.h \
	avx512vldqintrin.h \
	pkuintrin.h \
	avxintrin.h \
	bmi2intrin.h \
	bmiintrin.h \
	__clang_cuda_runtime_wrapper.h \
	cpuid.h \
	cuda_builtin_vars.h \
	emmintrin.h \
	f16cintrin.h \
	float.h \
	fma4intrin.h \
	fmaintrin.h \
	fxsrintrin.h \
	htmintrin.h \
	htmxlintrin.h \
	ia32intrin.h \
	immintrin.h \
	Intrin.h \
	inttypes.h \
	iso646.h \
	limits.h \
	lzcntintrin.h \
	mm3dnow.h \
	mmintrin.h \
	mm_malloc.h \
	module.modulemap \
	nmmintrin.h \
	pmmintrin.h \
	popcntintrin.h \
	prfchwintrin.h \
	rdseedintrin.h \
	rtmintrin.h \
	s390intrin.h \
	shaintrin.h \
	smmintrin.h \
	stdalign.h \
	stdarg.h \
	stdatomic.h \
	stdbool.h \
	stddef.h \
	__stddef_max_align_t.h \
	stdint.h \
	stdnoreturn.h \
	tbmintrin.h \
	tgmath.h \
	tmmintrin.h \
	unwind.h \
	vadefs.h \
	varargs.h \
	vecintrin.h \
	__wmmintrin_aes.h \
	wmmintrin.h \
	__wmmintrin_pclmul.h \
	x86intrin.h \
	xmmintrin.h \
	xopintrin.h \
	xsaveintrin.h \
	xsaveoptintrin.h \
	xsavecintrin.h \
	xsavesintrin.h \
	xtestintrin.h

all:	${HEADERS}

depend:
	# Nothing here so far ...

clean:
	# Nothing here so far ...

install includes: ${HEADERS}
	${INSTALL} -d -o ${SHAREOWN} -g ${SHAREGRP} -m ${DIRMODE} \
		${DESTDIR}${CLANG_INTR_INCDIR}
	${INSTALL} ${INSTALL_COPY} -m 444 ${.ALLSRC} \
		${DESTDIR}${CLANG_INTR_INCDIR}

.include <bsd.obj.mk>
