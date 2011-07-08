#  $OpenBSD: Makefile,v 1.89 2011/07/08 22:28:33 martynas Exp $
#  $NetBSD: Makefile,v 1.28 1995/11/20 22:06:19 jtc Exp $
#
#  @(#)Makefile 5.1beta 93/09/24 
# 
#  ====================================================
#  Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
# 
#  Developed at SunPro, a Sun Microsystems, Inc. business.
#  Permission to use, copy, modify, and distribute this
#  software is freely granted, provided that this notice 
#  is preserved.
#  ====================================================

.if (${MACHINE_ARCH} == "sparc")
CFLAGS+= -O0
.endif

.if (${MACHINE_ARCH} == "alpha")
.PATH:	${.CURDIR}/arch/alpha
ARCH_SRCS = s_copysign.S s_copysignf.S
.elif (${MACHINE_ARCH} == "i386")
.PATH:	${.CURDIR}/arch/i387
ARCH_SRCS = e_acos.S e_asin.S e_atan2.S e_exp.S e_fmod.S e_log.S e_log10.S \
	    e_remainder.S e_remainderf.S e_scalb.S e_sqrt.S e_sqrtf.S \
	    e_sqrtl.S \
	    invtrig.c \
	    s_atan.S s_atanf.S s_ceil.S s_ceilf.S s_copysign.S s_copysignf.S \
	    s_cos.S s_cosf.S s_floor.S s_floorf.S \
	    s_ilogb.S s_ilogbf.S s_log1p.S s_log1pf.S s_logb.S s_logbf.S \
	    s_llrint.S s_llrintf.S s_lrint.S s_lrintf.S s_rint.S s_rintf.S\
	    s_scalbn.S s_scalbnf.S s_significand.S s_significandf.S \
	    s_sin.S s_sinf.S s_tan.S s_tanf.S
.elif (${MACHINE_ARCH} == "amd64")
.PATH:	${.CURDIR}/arch/amd64
CPPFLAGS+=-I${.CURDIR}/arch/amd64
ARCH_SRCS = e_acos.S e_asin.S e_atan2.S e_exp.S e_fmod.S e_log.S e_log10.S \
	    e_remainder.S e_remainderf.S e_scalb.S e_sqrt.S e_sqrtf.S \
	    e_sqrtl.S \
	    invtrig.c \
	    s_atan.S s_atanf.S s_ceil.S s_ceilf.S s_copysign.S s_copysignf.S \
	    s_cos.S s_cosf.S s_floor.S s_floorf.S \
	    s_ilogb.S s_ilogbf.S s_log1p.S s_log1pf.S s_logb.S s_logbf.S \
	    s_llrint.S s_llrintf.S s_lrint.S s_lrintf.S \
	    s_rint.S s_rintf.S s_scalbn.S s_scalbnf.S s_significand.S \
	    s_significandf.S s_sin.S s_sinf.S s_tan.S s_tanf.S
.elif (${MACHINE_ARCH} == "m68k")
.PATH:	${.CURDIR}/arch/mc68881
ARCH_SRCS = e_acos.S e_acosf.S e_acosl.S e_asin.S e_asinf.S e_asinl.S \
	    e_atanh.S e_atanhf.S e_cosh.S e_coshf.S e_exp.S e_expf.S e_log.S \
	    e_logf.S e_log10.S e_log10f.S e_remainder.S e_remainderf.S \
	    e_scalb.S e_scalbf.S e_sinh.S e_sinhf.S e_sqrt.S e_sqrtf.S \
	    e_sqrtl.S s_atan.S s_atanf.S s_atanl.S s_ceil.S s_ceilf.S \
	    s_copysign.S s_copysignf.S s_copysignl.S s_cos.S s_cosf.S s_cosl.S \
	    s_expm1.S s_expm1f.S s_floor.S s_floorf.S s_log1p.S s_log1pf.S \
	    s_logb.S s_logbf.S s_logbl.S s_rint.S s_rintf.S s_rintl.S \
	    s_scalbn.S s_scalbnf.S s_scalbnl.S s_sin.S s_sinf.S s_sinl.S \
	    s_tan.S s_tanf.S s_tanl.S s_tanh.S s_tanhf.S
.elif (${MACHINE_ARCH} == "hppa")
.PATH:	${.CURDIR}/arch/hppa
ARCH_SRCS = e_sqrt.c e_sqrtf.c e_remainder.c e_remainderf.c \
	    s_ceil.c s_ceilf.c s_floor.c s_floorf.c s_rint.c s_rintf.c \
	    s_round.c s_roundf.c s_trunc.c s_truncf.c
.elif (${MACHINE_ARCH} == "sh")
.PATH:	${.CURDIR}/arch/sh
ARCH_SRCS = e_sqrt.c e_sqrtf.c s_fabsf.c
.elif (${MACHINE_ARCH} == "arm")
.PATH:	${.CURDIR}/arch/arm
.elif (${MACHINE_ARCH} == "hppa64")
.PATH:	${.CURDIR}/arch/hppa64
.elif (${MACHINE_ARCH} == "m88k")
.PATH:	${.CURDIR}/arch/m88k
.elif (${MACHINE_ARCH} == "mips64")
.PATH:	${.CURDIR}/arch/mips64
.elif (${MACHINE_ARCH} == "mips64el")
.PATH:	${.CURDIR}/arch/mips64
.elif (${MACHINE_ARCH} == "powerpc")
.PATH:	${.CURDIR}/arch/powerpc
.elif (${MACHINE_ARCH} == "sparc")
.PATH:	${.CURDIR}/arch/sparc
.elif (${MACHINE_ARCH} == "sparc64")
.PATH:	${.CURDIR}/arch/sparc64
.elif (${MACHINE_ARCH} == "vax")
.PATH:	${.CURDIR}/arch/vax
NOIEEE_ARCH = n_argred.S n_infnan.S n_sqrt.S
ARCH_SRCS = n_atan2.S n_cbrt.S n_hypot.S n_sincos.S n_support.S n_tan.S
.endif

.PATH:	${.CURDIR}/man
.PATH:	${.CURDIR}/src
.PATH:	${.CURDIR}/noieee_src

LIB=	m
WANTLINT=
COMMON_SRCS = b_exp__D.c b_log__D.c b_tgamma.c \
	e_acos.c e_acosf.c e_acosh.c e_acoshf.c e_asin.c e_asinf.c \
	e_atan2.c e_atan2f.c e_atanh.c e_atanhf.c e_cosh.c e_coshf.c e_exp.c \
	e_expf.c e_fmod.c e_fmodf.c e_hypot.c e_hypotf.c e_j0.c e_j0f.c \
	e_j1.c e_j1f.c e_jn.c e_jnf.c e_lgamma_r.c e_lgammaf_r.c e_log.c \
	e_log10.c e_log10f.c e_log2.c e_log2f.c e_logf.c e_pow.c e_powf.c \
	e_rem_pio2.c \
	e_rem_pio2f.c e_remainder.c e_remainderf.c e_scalb.c e_scalbf.c \
	e_sinh.c e_sinhf.c e_sqrt.c e_sqrtf.c \
	fenv.c \
	k_cos.c k_cosf.c k_rem_pio2.c k_rem_pio2f.c k_sin.c k_sinf.c \
	k_tan.c k_tanf.c \
	s_lround.c s_lroundf.c s_llround.c s_llroundf.c \
	s_asinh.c s_asinhf.c s_atan.c s_atanf.c s_cabs.c s_cabsf.c s_cacos.c \
	s_cacosf.c s_cacosh.c s_cacoshf.c s_carg.c s_cargf.c s_casin.c \
	s_casinf.c s_casinh.c s_casinhf.c s_catan.c s_catanf.c s_catanh.c \
	s_catanhf.c s_cbrt.c s_cbrtf.c s_conj.c s_conjf.c s_ccos.c s_ccosf.c \
	s_ccosh.c s_ccoshf.c s_ceil.c \
	s_ceilf.c s_cexp.c s_cexpf.c s_cimag.c s_cimagf.c s_clog.c s_clogf.c \
	s_copysign.c s_copysignf.c s_cos.c s_cosf.c s_cpow.c s_cpowf.c \
	s_cproj.c s_cprojf.c s_creal.c s_crealf.c s_csin.c s_csinf.c s_csinh.c \
	s_csinhf.c s_csqrt.c s_csqrtf.c s_ctan.c s_ctanf.c s_ctanh.c \
	s_ctanhf.c s_erf.c s_erff.c s_exp2.c s_exp2f.c s_expm1.c s_expm1f.c \
	s_fabsf.c s_fdim.c s_fma.c s_fmaf.c s_fmax.c s_fmaxf.c s_fmin.c \
	s_fminf.c s_floor.c s_floorf.c s_frexpf.c s_ilogb.c s_ilogbf.c \
	s_log1p.c \
	s_log1pf.c s_logb.c s_logbf.c s_llrint.c s_llrintf.c s_lrint.c \
	s_lrintf.c s_modff.c s_nan.c s_nearbyint.c s_nextafter.c \
	s_nextafterf.c s_nexttowardf.c s_remquo.c s_remquof.c s_rint.c \
	s_rintf.c s_round.c s_roundf.c \
	s_scalbln.c s_scalbn.c s_scalbnf.c s_signgam.c s_significand.c \
	s_significandf.c \
	s_sin.c s_sinf.c s_tan.c s_tanf.c s_tanh.c s_tanhf.c s_trunc.c \
	s_truncf.c w_drem.c w_dremf.c w_gamma.c w_gamma_r.c w_gammaf.c \
	w_gammaf_r.c w_lgamma.c w_lgammaf.c

LONG_SRCS = e_acoshl.c e_acosl.c e_asinl.c e_atan2l.c e_atanhl.c \
	e_coshl.c s_cabsl.c s_cacoshl.c s_cacosl.c s_cargl.c \
	s_casinhl.c s_casinl.c s_catanhl.c s_catanl.c s_ccoshl.c \
	s_ccosl.c s_cexpl.c s_cimagl.c s_clogl.c s_conjl.c s_cpowl.c \
	s_cprojl.c s_creall.c s_csinhl.c s_csinl.c s_csqrtl.c \
	s_ctanhl.c s_ctanl.c e_expl.c e_fmodl.c e_hypotl.c e_lgammal.c \
	e_log10l.c e_log2l.c e_logl.c e_powl.c e_remainderl.c \
	e_sinhl.c e_sqrtl.c e_tgammal.c invtrig.c k_cosl.c k_sinl.c \
	k_tanl.c polevll.c s_asinhl.c s_atanl.c s_cbrtl.c s_ceill.c \
	s_copysignl.c s_cosl.c s_erfl.c s_exp2l.c s_expm1l.c s_fabsl.c \
	s_floorl.c s_fmal.c s_fmaxl.c s_fminl.c s_frexpl.c s_ilogbl.c \
	s_llrintl.c s_llroundl.c s_log1pl.c s_logbl.c s_lrintl.c \
	s_lroundl.c s_modfl.c s_nanl.c s_nextafterl.c s_nexttoward.c \
	s_remquol.c s_rintl.c s_roundl.c s_scalbnl.c s_sinl.c \
	s_tanhl.c s_tanl.c s_truncl.c

# math routines for non-IEEE architectures.
NOIEEE_SRCS = n_acosh.c n_argred.c n_asincos.c n_asinh.c n_atan.c \
	n_atan2.c n_atanh.c n_cabs.c n_cacos.c n_cacosh.c n_carg.c \
	n_casin.c n_casinh.c n_catan.c n_catanh.c n_cbrt.c n_ccos.c \
	n_ccosh.c n_cexp.c n_cimag.c n_cimagf.c n_clog.c n_conj.c \
	n_conjf.c n_cosh.c n_cpow.c n_cproj.c n_cprojf.c n_creal.c \
	n_crealf.c n_csin.c n_csinh.c n_csqrt.c n_ctan.c n_ctanh.c \
	n_erf.c n_exp.c n_exp__E.c n_expm1.c n_fdim.c n_floor.c \
	n_fmax.c n_fmaxf.c n_fmin.c n_fminf.c n_fmod.c n_hypot.c \
	n_infnan.c n_j0.c n_j1.c n_jn.c n_lgamma.c n_log.c n_log10.c \
	n_log1p.c n_log__L.c n_nan.c n_pow.c n_round.c n_scalbln.c \
	n_sincos.c n_sinh.c n_sqrt.c n_support.c n_tan.c n_tanh.c \
	n_tgamma.c

# OpenBSD's C library supplies these functions:
#COMMON_SRCS+=	s_fabs.c s_frexp.c s_modf.c

.if (${MACHINE_ARCH} == "vax")
SRCS=	${NOIEEE_SRCS} ${NOIEEE_ARCH}
MAN+=	infnan.3
.else
SRCS=	${COMMON_SRCS}
.if (${MACHINE_ARCH} == "amd64") || (${MACHINE_ARCH} == "i386") || \
	(${MACHINE_ARCH} == "m68k") || (${MACHINE_ARCH} == "m88k")
.PATH:		${.CURDIR}/src/ld80
CPPFLAGS+=	-I${.CURDIR}/src -I${.CURDIR}/src/ld80
SRCS+=		${LONG_SRCS}
.endif
.if (${MACHINE_ARCH} == "hppa64") || (${MACHINE_CPU} == "mips64") || \
	(${MACHINE_ARCH} == "sparc64")
.PATH:		${.CURDIR}/src/ld128
CPPFLAGS+=	-I${.CURDIR}/src -I${.CURDIR}/src/ld128
SRCS+=		${LONG_SRCS}
.endif
.endif

# Substitute common sources with any arch specific sources 
.for i in ${ARCH_SRCS}
    SRCS:=${SRCS:S/${i:S/.S/.c/}/$i/}
    LSRCS+=${i:S/.S/.c/}
.endfor

.if (${MACHINE_ARCH} == "vax")
.for i in ${NOIEEE_ARCH}
    SRCS:=${SRCS:S/${i:S/.S/.c/}/$i/}
    LSRCS+=${i:S/.S/.c/}
.endfor
.endif

MAN+=	acos.3 acosh.3 asin.3 asinh.3 atan.3 atan2.3 atanh.3 ceil.3 \
	cimag.3 conj.3 copysign.3 cos.3 cosh.3 cproj.3 erf.3 exp.3 \
	fabs.3 fdim.3 feclearexcept.3 feenableexcept.3 fegetenv.3 \
	fegetround.3 floor.3 fmax.3 fmod.3 hypot.3 ilogb.3 j0.3 \
	lgamma.3 logb.3 lrint.3 lround.3 nan.3 nextafter.3 \
	remainder.3 rint.3 round.3 scalbn.3 sin.3 sinh.3 sqrt.3 \
	tan.3 tanh.3 trunc.3

MLINKS+=erf.3 erfc.3
MLINKS+=exp.3 exp2.3
MLINKS+=exp.3 expm1.3
MLINKS+=exp.3 log.3
MLINKS+=exp.3 log10.3
MLINKS+=exp.3 log1p.3
MLINKS+=exp.3 log2.3
MLINKS+=exp.3 pow.3
MLINKS+=feclearexcept.3 fegetexceptflag.3
MLINKS+=feclearexcept.3 feraiseexcept.3
MLINKS+=feclearexcept.3 fesetexceptflag.3
MLINKS+=feclearexcept.3 fetestexcept.3
MLINKS+=feenableexcept.3 fedisableexcept.3
MLINKS+=feenableexcept.3 fegetexcept.3
MLINKS+=fegetenv.3 feholdexcept.3
MLINKS+=fegetenv.3 fesetenv.3
MLINKS+=fegetenv.3 feupdateenv.3
MLINKS+=fegetround.3 fesetround.3
MLINKS+=fmax.3 fmin.3
MLINKS+=hypot.3 cabs.3
MLINKS+=j0.3 j1.3 j0.3 jn.3 j0.3 y0.3 j0.3 y1.3 j0.3 yn.3
MLINKS+=lgamma.3 gamma.3 lgamma.3 tgamma.3
MLINKS+=logb.3 scalb.3
MLINKS+=logb.3 significand.3
MLINKS+=lrint.3 llrint.3
MLINKS+=lround.3 llround.3
MLINKS+=remainder.3 drem.3 remainder.3 remquo.3
MLINKS+=rint.3 nearbyint.3
MLINKS+=scalbn.3 scalbln.3
MLINKS+=sqrt.3 cbrt.3

# float versions
MLINKS+=acos.3 acosf.3
MLINKS+=acosh.3 acoshf.3
MLINKS+=asin.3 asinf.3
MLINKS+=asinh.3 asinhf.3
MLINKS+=atan.3 atanf.3
MLINKS+=atan2.3 atan2f.3
MLINKS+=atanh.3 atanhf.3
MLINKS+=ceil.3 ceilf.3
MLINKS+=cimag.3 cimagf.3
MLINKS+=conj.3 conjf.3
MLINKS+=copysign.3 copysignf.3
MLINKS+=cos.3 cosf.3
MLINKS+=cosh.3 coshf.3
MLINKS+=cproj.3 cprojf.3
MLINKS+=erf.3 erfcf.3
MLINKS+=erf.3 erff.3
MLINKS+=exp.3 exp2f.3
MLINKS+=exp.3 expf.3
MLINKS+=exp.3 expm1f.3
MLINKS+=exp.3 log10f.3
MLINKS+=exp.3 log1pf.3
MLINKS+=exp.3 log2f.3
MLINKS+=exp.3 logf.3
MLINKS+=exp.3 powf.3
MLINKS+=fabs.3 fabsf.3
MLINKS+=fdim.3 fdimf.3
MLINKS+=floor.3 floorf.3
MLINKS+=fmax.3 fmaxf.3
MLINKS+=fmax.3 fminf.3
MLINKS+=fmod.3 fmodf.3
MLINKS+=hypot.3 cabsf.3
MLINKS+=hypot.3 hypotf.3
MLINKS+=ilogb.3 ilogbf.3
MLINKS+=j0.3 j0f.3
MLINKS+=j0.3 j1f.3
MLINKS+=j0.3 jnf.3
MLINKS+=j0.3 y0f.3
MLINKS+=j0.3 y1f.3
MLINKS+=j0.3 ynf.3
MLINKS+=lgamma.3 gammaf.3
MLINKS+=lgamma.3 lgammaf.3
MLINKS+=lgamma.3 tgammaf.3
MLINKS+=logb.3 logbf.3
MLINKS+=logb.3 scalbf.3
MLINKS+=logb.3 significandf.3
MLINKS+=lrint.3 llrintf.3
MLINKS+=lrint.3 lrintf.3
MLINKS+=lround.3 llroundf.3
MLINKS+=lround.3 lroundf.3
MLINKS+=nan.3 nanf.3
MLINKS+=nearbyint.3 nearbyintf.3
MLINKS+=nextafter.3 nextafterf.3
MLINKS+=remainder.3 dremf.3
MLINKS+=remainder.3 remainderf.3
MLINKS+=remainder.3 remquof.3
MLINKS+=rint.3 rintf.3
MLINKS+=round.3 roundf.3
MLINKS+=scalbln.3 scalblnf.3
MLINKS+=scalbn.3 scalbnf.3
MLINKS+=sin.3 sinf.3
MLINKS+=sinh.3 sinhf.3
MLINKS+=sqrt.3 cbrtf.3
MLINKS+=sqrt.3 sqrtf.3
MLINKS+=tan.3 tanf.3
MLINKS+=tanh.3 tanhf.3
MLINKS+=trunc.3 truncf.3

# long double versions
MLINKS+=acos.3 acosl.3
MLINKS+=acosh.3 acoshl.3
MLINKS+=asin.3 asinl.3
MLINKS+=asinh.3 asinhl.3
MLINKS+=atan.3 atanl.3
MLINKS+=atan2.3 atan2l.3
MLINKS+=atanh.3 atanhl.3
MLINKS+=ceil.3 ceill.3
MLINKS+=copysign.3 copysignl.3
MLINKS+=cos.3 cosl.3
MLINKS+=cosh.3 coshl.3
MLINKS+=erf.3 erfcl.3
MLINKS+=erf.3 erfl.3
MLINKS+=exp.3 exp2l.3
MLINKS+=exp.3 expl.3
MLINKS+=exp.3 expm1l.3
MLINKS+=exp.3 log10l.3
MLINKS+=exp.3 log1pl.3
MLINKS+=exp.3 log2l.3
MLINKS+=exp.3 logl.3
MLINKS+=exp.3 powl.3
MLINKS+=fabs.3 fabsl.3
MLINKS+=fdim.3 fdiml.3
MLINKS+=floor.3 floorl.3
MLINKS+=fmax.3 fmaxl.3
MLINKS+=fmax.3 fminl.3
MLINKS+=fmod.3 fmodl.3
MLINKS+=hypot.3 hypotl.3
MLINKS+=ilogb.3 ilogbl.3
MLINKS+=lgamma.3 lgammal.3
MLINKS+=lgamma.3 tgammal.3
MLINKS+=logb.3 logbl.3
MLINKS+=logb.3 scalbl.3
MLINKS+=lrint.3 llrintl.3
MLINKS+=lrint.3 lrintl.3
MLINKS+=lround.3 llroundl.3
MLINKS+=lround.3 lroundl.3
MLINKS+=nan.3 nanl.3
MLINKS+=nearbyint.3 nearbyintl.3
MLINKS+=nextafter.3 nextafterl.3
MLINKS+=remainder.3 remainderl.3
MLINKS+=remainder.3 remquol.3
MLINKS+=rint.3 rintl.3
MLINKS+=round.3 roundl.3
MLINKS+=scalbln.3 scalblnl.3
MLINKS+=scalbn.3 scalbnl.3
MLINKS+=sin.3 sinl.3
MLINKS+=sinh.3 sinhl.3
MLINKS+=sqrt.3 cbrtl.3
MLINKS+=sqrt.3 sqrtl.3
MLINKS+=tan.3 tanl.3
MLINKS+=tanh.3 tanhl.3
MLINKS+=trunc.3 truncl.3

.include <bsd.lib.mk>
