#  $OpenBSD: Makefile,v 1.1 1996/08/09 00:59:29 downsj Exp $
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
# 
# 

#
# There are two options in making libm at fdlibm compile time:
# 	_IEEE_LIBM 	--- IEEE libm; smaller, and somewhat faster
#	_MULTI_LIBM	--- Support multi-standard at runtime by 
#			    imposing wrapper functions defined in 
#			    fdlibm.h:
#				_IEEE_MODE 	-- IEEE
#				_XOPEN_MODE 	-- X/OPEN
#				_POSIX_MODE 	-- POSIX/ANSI
#				_SVID3_MODE 	-- SVID
#
# Here is how to set up CFLAGS to create the desired libm at 
# compile time:
#
# 	CFLAGS = -D_IEEE_LIBM		... IEEE libm (recommended)
#	CFLAGS = -D_SVID3_MODE	... Multi-standard supported
#					    libm with SVID as the 
#					    default standard
#	CFLAGS = -D_XOPEN_MODE	... Multi-standard supported
#					    libm with XOPEN as the 
#					    default standard
#	CFLAGS = -D_POSIX_MODE	... Multi-standard supported
#					    libm with POSIX as the 
#					    default standard
#	CFLAGS = 			... Multi-standard supported
#					    libm with IEEE as the 
#					    default standard
# 

.PATH:	${.CURDIR}/../libm/arch/i387
ARCH_SRCS = e_acos.S e_asin.S e_atan2.S e_exp.S e_fmod.S e_log.S e_log10.S \
	    e_remainder.S e_remainderf.S e_scalb.S e_sqrt.S e_sqrtf.S \
	    s_atan.S s_atanf.S s_ceil.S s_ceilf.S s_copysign.S s_copysignf.S \
	    s_cos.S s_cosf.S s_finite.S s_finitef.S s_floor.S s_floorf.S \
	    s_ilogb.S s_ilogbf.S s_log1p.S s_log1pf.S s_logb.S s_logbf.S \
	    s_rint.S s_rintf.S s_scalbn.S s_scalbnf.S s_significand.S \
	    s_significandf.S s_sin.S s_sinf.S s_tan.S s_tanf.S

.PATH:	${.CURDIR}/../libm/src
.PATH:	${.CURDIR}/../libm/noieee_src

CFLAGS+= -D_MULTI_LIBM -D_POSIX_MODE

LIB=	m_i387
COMMON_SRCS = e_acos.c e_acosf.c e_acosh.c e_acoshf.c e_asin.c e_asinf.c \
	e_atan2.c e_atan2f.c e_atanh.c e_atanhf.c e_cosh.c e_coshf.c e_exp.c \
	e_expf.c e_fmod.c e_fmodf.c e_hypot.c e_hypotf.c e_j0.c e_j0f.c \
	e_j1.c e_j1f.c e_jn.c e_jnf.c e_lgamma_r.c e_lgammaf_r.c e_log.c \
	e_log10.c e_log10f.c e_logf.c e_pow.c e_powf.c e_rem_pio2.c \
	e_rem_pio2f.c e_remainder.c e_remainderf.c e_scalb.c e_scalbf.c \
	e_sinh.c e_sinhf.c e_sqrt.c e_sqrtf.c \
	k_cos.c k_cosf.c k_rem_pio2.c k_rem_pio2f.c k_sin.c k_sinf.c \
	k_standard.c k_tan.c k_tanf.c \
	s_asinh.c s_asinhf.c s_atan.c s_atanf.c s_cbrt.c s_cbrtf.c s_ceil.c \
	s_ceilf.c s_copysign.c s_copysignf.c s_cos.c s_cosf.c s_erf.c s_erff.c \
	s_expm1.c s_expm1f.c s_fabsf.c s_finite.c s_finitef.c \
	s_floor.c s_floorf.c s_frexpf.c s_ilogb.c s_ilogbf.c \
	s_isinff.c s_isnanf.c s_ldexpf.c s_lib_version.c s_log1p.c \
	s_log1pf.c s_logb.c s_logbf.c s_matherr.c s_modff.c \
	s_nextafter.c s_nextafterf.c s_rint.c s_rintf.c s_scalbn.c s_scalbnf.c \
	s_signgam.c s_significand.c s_significandf.c s_sin.c s_sinf.c s_tan.c \
	s_tanf.c s_tanh.c s_tanhf.c \
	w_acos.c w_acosf.c w_acosh.c w_acoshf.c w_asin.c w_asinf.c w_atan2.c \
	w_atan2f.c w_atanh.c w_atanhf.c w_cabs.c w_cabsf.c w_cosh.c w_coshf.c \
	w_drem.c w_dremf.c w_exp.c w_expf.c w_fmod.c w_fmodf.c w_gamma.c \
	w_gamma_r.c w_gammaf.c w_gammaf_r.c w_hypot.c w_hypotf.c w_j0.c \
	w_j0f.c w_j1.c w_j1f.c w_jn.c w_jnf.c w_lgamma.c w_lgamma_r.c \
	w_lgammaf.c w_lgammaf_r.c w_log.c w_log10.c w_log10f.c w_logf.c \
	w_pow.c w_powf.c w_remainder.c w_remainderf.c w_scalb.c w_scalbf.c \
	w_sinh.c w_sinhf.c w_sqrt.c w_sqrtf.c

# math routines for non-IEEE architectures.
NOIEEE_SRCS = n_asincos.c n_acosh.c n_asinh.c n_atan.c n_atanh.c n_cosh.c \
	n_erf.c n_exp.c n_exp__E.c n_expm1.c n_floor.c n_fmod.c n_gamma.c \
	n_lgamma.c n_j0.c n_j1.c n_jn.c n_log.c n_log10.c n_log1p.c \
	n_log__L.c n_pow.c n_sinh.c n_tanh.c n_atan2.c n_cabs.c n_cbrt.c \
	n_sqrt.c n_sincos.c n_tan.c n_argred.c n_support.c n_infnan.c
        

# NetBSD's C library supplies these functions:
#COMMON_SRCS+=	s_fabs.c s_frexp.c s_isinf.c s_isnan.c s_ldexp.c s_modf.c

SRCS=	${COMMON_SRCS}

# Substitute common sources with any arch specific sources 
.for i in ${ARCH_SRCS}
    SRCS:=${SRCS:S/${i:S/.S/.c/}/$i/}
    LSRCS+=${i:S/.S/.c/}
.endfor

.include <bsd.lib.mk>
