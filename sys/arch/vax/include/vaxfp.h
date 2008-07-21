/*	$OpenBSD: vaxfp.h,v 1.1 2008/07/21 21:50:06 martynas Exp $	*/
/*	$NetBSD: vaxfp.h,v 1.7 2008/04/28 20:23:39 martin Exp $	*/

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Klaus Klein.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * vaxfp.h defines the layout of VAX Floating-Point data types.
 * Only F_floating and D_floating types are defined here;
 * G_floating and H_floating are not supported by OpenBSD.
 */
#ifndef _VAX_VAXFP_H_
#define	_VAX_VAXFP_H_

#define	FFLT_EXPBITS	8
#define	FFLT_FRACHBITS	7
#define	FFLT_FRACLBITS	16
#define	FFLT_FRACBITS	(FFLT_FRACLBITS + FFLT_FRACHBITS)

struct vax_f_floating {
	unsigned int	fflt_frach:FFLT_FRACHBITS;
	unsigned int	fflt_exp:FFLT_EXPBITS;
	unsigned int	fflt_sign:1;
	unsigned int	fflt_fracl:FFLT_FRACLBITS;
};

#define	DFLT_EXPBITS	8
#define	DFLT_FRACHBITS	7
#define	DFLT_FRACMBITS	16
#define	DFLT_FRACLBITS	32
#define	DFLT_FRACBITS	(DFLT_FRACLBITS + DFLT_FRACMBITS + DFLT_FRACHBITS)

struct vax_d_floating {
	unsigned int	dflt_frach:DFLT_FRACHBITS;
	unsigned int	dflt_exp:DFLT_EXPBITS;
	unsigned int	dflt_sign:1;
	unsigned int	dflt_fracm:DFLT_FRACMBITS;
	unsigned int	dflt_fracl:DFLT_FRACLBITS;
};

/*
 * Exponent biases.
 */
#define	FFLT_EXP_BIAS	128
#define	DFLT_EXP_BIAS	128

/*
 * Convenience data structures.
 */
union vax_ffloating_u {
	float			ffltu_f;
	struct vax_f_floating	ffltu_fflt;
};

union vax_dfloating_u {
	double			dfltu_d;
	struct vax_d_floating	dfltu_dflt;
};

#endif /* _VAX_VAXFP_H_ */
