/*	$NetBSD: fpu_log.c,v 1.2 1995/11/05 00:35:31 briggs Exp $	*/

/*
 * Copyright (c) 1995  Ken Nakata
 *	All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)fpu_log.c	10/8/95
 */

#include <sys/types.h>

#include "fpu_emulate.h"

static u_int logA6[] = { 0x3FC2499A, 0xB5E4040B };
static u_int logA5[] = { 0xBFC555B5, 0x848CB7DB };
static u_int logA4[] = { 0x3FC99999, 0x987D8730 };
static u_int logA3[] = { 0xBFCFFFFF, 0xFF6F7E97 };
static u_int logA2[] = { 0x3FD55555, 0x555555A4 };
static u_int logA1[] = { 0xBFE00000, 0x00000008 };

static u_int logB5[] = { 0x3F175496, 0xADD7DAD6 };
static u_int logB4[] = { 0x3F3C71C2, 0xFE80C7E0 };
static u_int logB3[] = { 0x3F624924, 0x928BCCFF };
static u_int logB2[] = { 0x3F899999, 0x999995EC };
static u_int logB1[] = { 0x3FB55555, 0x55555555 };

/* sfpn = shortened fp number; can represent only positive numbers */
static struct sfpn {
    int		sp_exp;
    u_int	sp_m0, sp_m1;
} logtbl[] = {
    { 0x3FFE - 0x3fff, 0xFE03F80FU, 0xE03F80FEU },
    { 0x3FF7 - 0x3fff, 0xFF015358U, 0x833C47E2U },
    { 0x3FFE - 0x3fff, 0xFA232CF2U, 0x52138AC0U },
    { 0x3FF9 - 0x3fff, 0xBDC8D83EU, 0xAD88D549U },
    { 0x3FFE - 0x3fff, 0xF6603D98U, 0x0F6603DAU },
    { 0x3FFA - 0x3fff, 0x9CF43DCFU, 0xF5EAFD48U },
    { 0x3FFE - 0x3fff, 0xF2B9D648U, 0x0F2B9D65U },
    { 0x3FFA - 0x3fff, 0xDA16EB88U, 0xCB8DF614U },
    { 0x3FFE - 0x3fff, 0xEF2EB71FU, 0xC4345238U },
    { 0x3FFB - 0x3fff, 0x8B29B775U, 0x1BD70743U },
    { 0x3FFE - 0x3fff, 0xEBBDB2A5U, 0xC1619C8CU },
    { 0x3FFB - 0x3fff, 0xA8D839F8U, 0x30C1FB49U },
    { 0x3FFE - 0x3fff, 0xE865AC7BU, 0x7603A197U },
    { 0x3FFB - 0x3fff, 0xC61A2EB1U, 0x8CD907ADU },
    { 0x3FFE - 0x3fff, 0xE525982AU, 0xF70C880EU },
    { 0x3FFB - 0x3fff, 0xE2F2A47AU, 0xDE3A18AFU },
    { 0x3FFE - 0x3fff, 0xE1FC780EU, 0x1FC780E2U },
    { 0x3FFB - 0x3fff, 0xFF64898EU, 0xDF55D551U },
    { 0x3FFE - 0x3fff, 0xDEE95C4CU, 0xA037BA57U },
    { 0x3FFC - 0x3fff, 0x8DB956A9U, 0x7B3D0148U },
    { 0x3FFE - 0x3fff, 0xDBEB61EEU, 0xD19C5958U },
    { 0x3FFC - 0x3fff, 0x9B8FE100U, 0xF47BA1DEU },
    { 0x3FFE - 0x3fff, 0xD901B203U, 0x6406C80EU },
    { 0x3FFC - 0x3fff, 0xA9372F1DU, 0x0DA1BD17U },
    { 0x3FFE - 0x3fff, 0xD62B80D6U, 0x2B80D62CU },
    { 0x3FFC - 0x3fff, 0xB6B07F38U, 0xCE90E46BU },
    { 0x3FFE - 0x3fff, 0xD3680D36U, 0x80D3680DU },
    { 0x3FFC - 0x3fff, 0xC3FD0329U, 0x06488481U },
    { 0x3FFE - 0x3fff, 0xD0B69FCBU, 0xD2580D0BU },
    { 0x3FFC - 0x3fff, 0xD11DE0FFU, 0x15AB18CAU },
    { 0x3FFE - 0x3fff, 0xCE168A77U, 0x25080CE1U },
    { 0x3FFC - 0x3fff, 0xDE1433A1U, 0x6C66B150U },
    { 0x3FFE - 0x3fff, 0xCB8727C0U, 0x65C393E0U },
    { 0x3FFC - 0x3fff, 0xEAE10B5AU, 0x7DDC8ADDU },
    { 0x3FFE - 0x3fff, 0xC907DA4EU, 0x871146ADU },
    { 0x3FFC - 0x3fff, 0xF7856E5EU, 0xE2C9B291U },
    { 0x3FFE - 0x3fff, 0xC6980C69U, 0x80C6980CU },
    { 0x3FFD - 0x3fff, 0x82012CA5U, 0xA68206D7U },
    { 0x3FFE - 0x3fff, 0xC4372F85U, 0x5D824CA6U },
    { 0x3FFD - 0x3fff, 0x882C5FCDU, 0x7256A8C5U },
    { 0x3FFE - 0x3fff, 0xC1E4BBD5U, 0x95F6E947U },
    { 0x3FFD - 0x3fff, 0x8E44C60BU, 0x4CCFD7DEU },
    { 0x3FFE - 0x3fff, 0xBFA02FE8U, 0x0BFA02FFU },
    { 0x3FFD - 0x3fff, 0x944AD09EU, 0xF4351AF6U },
    { 0x3FFE - 0x3fff, 0xBD691047U, 0x07661AA3U },
    { 0x3FFD - 0x3fff, 0x9A3EECD4U, 0xC3EAA6B2U },
    { 0x3FFE - 0x3fff, 0xBB3EE721U, 0xA54D880CU },
    { 0x3FFD - 0x3fff, 0xA0218434U, 0x353F1DE8U },
    { 0x3FFE - 0x3fff, 0xB92143FAU, 0x36F5E02EU },
    { 0x3FFD - 0x3fff, 0xA5F2FCABU, 0xBBC506DAU },
    { 0x3FFE - 0x3fff, 0xB70FBB5AU, 0x19BE3659U },
    { 0x3FFD - 0x3fff, 0xABB3B8BAU, 0x2AD362A5U },
    { 0x3FFE - 0x3fff, 0xB509E68AU, 0x9B94821FU },
    { 0x3FFD - 0x3fff, 0xB1641795U, 0xCE3CA97BU },
    { 0x3FFE - 0x3fff, 0xB30F6352U, 0x8917C80BU },
    { 0x3FFD - 0x3fff, 0xB7047551U, 0x5D0F1C61U },
    { 0x3FFE - 0x3fff, 0xB11FD3B8U, 0x0B11FD3CU },
    { 0x3FFD - 0x3fff, 0xBC952AFEU, 0xEA3D13E1U },
    { 0x3FFE - 0x3fff, 0xAF3ADDC6U, 0x80AF3ADEU },
    { 0x3FFD - 0x3fff, 0xC2168ED0U, 0xF458BA4AU },
    { 0x3FFE - 0x3fff, 0xAD602B58U, 0x0AD602B6U },
    { 0x3FFD - 0x3fff, 0xC788F439U, 0xB3163BF1U },
    { 0x3FFE - 0x3fff, 0xAB8F69E2U, 0x8359CD11U },
    { 0x3FFD - 0x3fff, 0xCCECAC08U, 0xBF04565DU },
    { 0x3FFE - 0x3fff, 0xA9C84A47U, 0xA07F5638U },
    { 0x3FFD - 0x3fff, 0xD2420487U, 0x2DD85160U },
    { 0x3FFE - 0x3fff, 0xA80A80A8U, 0x0A80A80BU },
    { 0x3FFD - 0x3fff, 0xD7894992U, 0x3BC3588AU },
    { 0x3FFE - 0x3fff, 0xA655C439U, 0x2D7B73A8U },
    { 0x3FFD - 0x3fff, 0xDCC2C4B4U, 0x9887DACCU },
    { 0x3FFE - 0x3fff, 0xA4A9CF1DU, 0x96833751U },
    { 0x3FFD - 0x3fff, 0xE1EEBD3EU, 0x6D6A6B9EU },
    { 0x3FFE - 0x3fff, 0xA3065E3FU, 0xAE7CD0E0U },
    { 0x3FFD - 0x3fff, 0xE70D785CU, 0x2F9F5BDCU },
    { 0x3FFE - 0x3fff, 0xA16B312EU, 0xA8FC377DU },
    { 0x3FFD - 0x3fff, 0xEC1F392CU, 0x5179F283U },
    { 0x3FFE - 0x3fff, 0x9FD809FDU, 0x809FD80AU },
    { 0x3FFD - 0x3fff, 0xF12440D3U, 0xE36130E6U },
    { 0x3FFE - 0x3fff, 0x9E4CAD23U, 0xDD5F3A20U },
    { 0x3FFD - 0x3fff, 0xF61CCE92U, 0x346600BBU },
    { 0x3FFE - 0x3fff, 0x9CC8E160U, 0xC3FB19B9U },
    { 0x3FFD - 0x3fff, 0xFB091FD3U, 0x8145630AU },
    { 0x3FFE - 0x3fff, 0x9B4C6F9EU, 0xF03A3CAAU },
    { 0x3FFD - 0x3fff, 0xFFE97042U, 0xBFA4C2ADU },
    { 0x3FFE - 0x3fff, 0x99D722DAU, 0xBDE58F06U },
    { 0x3FFE - 0x3fff, 0x825EFCEDU, 0x49369330U },
    { 0x3FFE - 0x3fff, 0x9868C809U, 0x868C8098U },
    { 0x3FFE - 0x3fff, 0x84C37A7AU, 0xB9A905C9U },
    { 0x3FFE - 0x3fff, 0x97012E02U, 0x5C04B809U },
    { 0x3FFE - 0x3fff, 0x87224C2EU, 0x8E645FB7U },
    { 0x3FFE - 0x3fff, 0x95A02568U, 0x095A0257U },
    { 0x3FFE - 0x3fff, 0x897B8CACU, 0x9F7DE298U },
    { 0x3FFE - 0x3fff, 0x94458094U, 0x45809446U },
    { 0x3FFE - 0x3fff, 0x8BCF55DEU, 0xC4CD05FEU },
    { 0x3FFE - 0x3fff, 0x92F11384U, 0x0497889CU },
    { 0x3FFE - 0x3fff, 0x8E1DC0FBU, 0x89E125E5U },
    { 0x3FFE - 0x3fff, 0x91A2B3C4U, 0xD5E6F809U },
    { 0x3FFE - 0x3fff, 0x9066E68CU, 0x955B6C9BU },
    { 0x3FFE - 0x3fff, 0x905A3863U, 0x3E06C43BU },
    { 0x3FFE - 0x3fff, 0x92AADE74U, 0xC7BE59E0U },
    { 0x3FFE - 0x3fff, 0x8F1779D9U, 0xFDC3A219U },
    { 0x3FFE - 0x3fff, 0x94E9BFF6U, 0x15845643U },
    { 0x3FFE - 0x3fff, 0x8DDA5202U, 0x37694809U },
    { 0x3FFE - 0x3fff, 0x9723A1B7U, 0x20134203U },
    { 0x3FFE - 0x3fff, 0x8CA29C04U, 0x6514E023U },
    { 0x3FFE - 0x3fff, 0x995899C8U, 0x90EB8990U },
    { 0x3FFE - 0x3fff, 0x8B70344AU, 0x139BC75AU },
    { 0x3FFE - 0x3fff, 0x9B88BDAAU, 0x3A3DAE2FU },
    { 0x3FFE - 0x3fff, 0x8A42F870U, 0x5669DB46U },
    { 0x3FFE - 0x3fff, 0x9DB4224FU, 0xFFE1157CU },
    { 0x3FFE - 0x3fff, 0x891AC73AU, 0xE9819B50U },
    { 0x3FFE - 0x3fff, 0x9FDADC26U, 0x8B7A12DAU },
    { 0x3FFE - 0x3fff, 0x87F78087U, 0xF78087F8U },
    { 0x3FFE - 0x3fff, 0xA1FCFF17U, 0xCE733BD4U },
    { 0x3FFE - 0x3fff, 0x86D90544U, 0x7A34ACC6U },
    { 0x3FFE - 0x3fff, 0xA41A9E8FU, 0x5446FB9FU },
    { 0x3FFE - 0x3fff, 0x85BF3761U, 0x2CEE3C9BU },
    { 0x3FFE - 0x3fff, 0xA633CD7EU, 0x6771CD8BU },
    { 0x3FFE - 0x3fff, 0x84A9F9C8U, 0x084A9F9DU },
    { 0x3FFE - 0x3fff, 0xA8489E60U, 0x0B435A5EU },
    { 0x3FFE - 0x3fff, 0x83993052U, 0x3FBE3368U },
    { 0x3FFE - 0x3fff, 0xAA59233CU, 0xCCA4BD49U },
    { 0x3FFE - 0x3fff, 0x828CBFBEU, 0xB9A020A3U },
    { 0x3FFE - 0x3fff, 0xAC656DAEU, 0x6BCC4985U },
    { 0x3FFE - 0x3fff, 0x81848DA8U, 0xFAF0D277U },
    { 0x3FFE - 0x3fff, 0xAE6D8EE3U, 0x60BB2468U },
    { 0x3FFE - 0x3fff, 0x80808080U, 0x80808081U },
    { 0x3FFE - 0x3fff, 0xB07197A2U, 0x3C46C654U },
};

static struct fpn *__fpu_logn __P((struct fpemu *fe));

/*
 * natural log - algorithm taken from Motorola FPSP,
 * except this doesn't bother to check for invalid input.
 */
static struct fpn *
__fpu_logn(fe)
     struct fpemu *fe;
{
    static struct fpn X, F, U, V, W, KLOG2;
    struct fpn *d;
    int i, k;

    CPYFPN(&X, &fe->fe_f2);

    /* see if |X-1| < 1/16 approx. */
    if ((-1 == X.fp_exp && (0xf07d0000U >> (31 - FP_LG)) <= X.fp_mant[0]) ||
	(0 == X.fp_exp && X.fp_mant[0] <= (0x88410000U >> (31 - FP_LG)))) {
	/* log near 1 */
	if (fpu_debug_level & DL_ARITH)
	    printf("__fpu_logn: log near 1\n");

	fpu_const(&fe->fe_f1, 0x32);
	/* X+1 */
	d = fpu_add(fe);
	CPYFPN(&V, d);

	CPYFPN(&fe->fe_f1, &X);
	fpu_const(&fe->fe_f2, 0x32); /* 1.0 */
	fe->fe_f2.fp_sign = 1; /* -1.0 */
	/* X-1 */
	d = fpu_add(fe);
	CPYFPN(&fe->fe_f1, d);
	/* 2(X-1) */
	fe->fe_f1.fp_exp++; /* *= 2 */
	CPYFPN(&fe->fe_f2, &V);
	/* U=2(X-1)/(X+1) */
	d = fpu_div(fe);
	CPYFPN(&U, d);
	CPYFPN(&fe->fe_f1, d);
	CPYFPN(&fe->fe_f2, d);
	/* V=U*U */
	d = fpu_mul(fe);
	CPYFPN(&V, d);
	CPYFPN(&fe->fe_f1, d);
	CPYFPN(&fe->fe_f2, d);
	/* W=V*V */
	d = fpu_mul(fe);
	CPYFPN(&W, d);

	/* calculate U+U*V*([B1+W*(B3+W*B5)]+[V*(B2+W*B4)]) */

	/* B1+W*(B3+W*B5) part */
	CPYFPN(&fe->fe_f1, d);
	fpu_explode(fe, &fe->fe_f2, FTYPE_DBL, logB5);
	/* W*B5 */
	d = fpu_mul(fe);
	CPYFPN(&fe->fe_f1, d);
	fpu_explode(fe, &fe->fe_f2, FTYPE_DBL, logB3);
	/* B3+W*B5 */
	d = fpu_add(fe);
	CPYFPN(&fe->fe_f1, d);
	CPYFPN(&fe->fe_f2, &W);
	/* W*(B3+W*B5) */
	d = fpu_mul(fe);
	CPYFPN(&fe->fe_f1, d);
	fpu_explode(fe, &fe->fe_f2, FTYPE_DBL, logB1);
	/* B1+W*(B3+W*B5) */
	d = fpu_add(fe);
	CPYFPN(&X, d);

	/* [V*(B2+W*B4)] part */
	CPYFPN(&fe->fe_f1, &W);
	fpu_explode(fe, &fe->fe_f2, FTYPE_DBL, logB4);
	/* W*B4 */
	d = fpu_mul(fe);
	CPYFPN(&fe->fe_f1, d);
	fpu_explode(fe, &fe->fe_f2, FTYPE_DBL, logB2);
	/* B2+W*B4 */
	d = fpu_add(fe);
	CPYFPN(&fe->fe_f1, d);
	CPYFPN(&fe->fe_f2, &V);
	/* V*(B2+W*B4) */
	d = fpu_mul(fe);
	CPYFPN(&fe->fe_f1, d);
	CPYFPN(&fe->fe_f2, &X);
	/* B1+W*(B3+W*B5)+V*(B2+W*B4) */
	d = fpu_add(fe);
	CPYFPN(&fe->fe_f1, d);
	CPYFPN(&fe->fe_f2, &V);
	/* V*(B1+W*(B3+W*B5)+V*(B2+W*B4)) */
	d = fpu_mul(fe);
	CPYFPN(&fe->fe_f1, d);
	CPYFPN(&fe->fe_f2, &U);
	/* U*V*(B1+W*(B3+W*B5)+V*(B2+W*B4)) */
	d = fpu_mul(fe);
	CPYFPN(&fe->fe_f1, d);
	CPYFPN(&fe->fe_f2, &U);
	/* U+U*V*(B1+W*(B3+W*B5)+V*(B2+W*B4)) */
	d = fpu_add(fe);
    } else /* the usual case */ {
	if (fpu_debug_level & DL_ARITH)
	    printf("__fpu_logn: the usual case. X=(%d,%08x,%08x...)\n",
		   X.fp_exp, X.fp_mant[0], X.fp_mant[1]);

	k = X.fp_exp;
	/* X <- Y */
	X.fp_exp = fe->fe_f2.fp_exp = 0;

	/* get the most significant 7 bits of X */
	F.fp_class = FPC_NUM;
	F.fp_sign = 0;
	F.fp_exp = X.fp_exp;
	F.fp_mant[0] = X.fp_mant[0] & (0xfe000000U >> (31 - FP_LG));
	F.fp_mant[0] |= (0x01000000U >> (31 - FP_LG));
	F.fp_mant[1] = F.fp_mant[2] = F.fp_mant[3] = 0;
	F.fp_sticky = 0;

	if (fpu_debug_level & DL_ARITH) {
	    printf("__fpu_logn: X=Y*2^k=(%d,%08x,%08x...)*2^%d\n",
		   fe->fe_f2.fp_exp, fe->fe_f2.fp_mant[0],
		   fe->fe_f2.fp_mant[1], k);
	    printf("__fpu_logn: F=(%d,%08x,%08x...)\n",
		   F.fp_exp, F.fp_mant[0], F.fp_mant[1]);
	}

	/* index to the table */
	i = (F.fp_mant[0] >> (FP_LG - 7)) & 0x7e;

	if (fpu_debug_level & DL_ARITH)
	    printf("__fpu_logn: index to logtbl i=%d(%x)\n", i, i);

	CPYFPN(&fe->fe_f1, &F);
	/* -F */
	fe->fe_f1.fp_sign = 1;
	/* Y-F */
	d = fpu_add(fe);
	CPYFPN(&fe->fe_f1, d);

	/* fe_f2 = 1/F */
	fe->fe_f2.fp_class = FPC_NUM;
	fe->fe_f2.fp_sign = fe->fe_f2.fp_sticky = fe->fe_f2.fp_mant[3] = 0;
	fe->fe_f2.fp_exp = logtbl[i].sp_exp;
	fe->fe_f2.fp_mant[0] = (logtbl[i].sp_m0 >> (31 - FP_LG));
	fe->fe_f2.fp_mant[1] = (logtbl[i].sp_m0 << (FP_LG + 1)) |
	    (logtbl[i].sp_m1 >> (31 - FP_LG));
	fe->fe_f2.fp_mant[2] = (u_int)(logtbl[i].sp_m1 << (FP_LG + 1));

	if (fpu_debug_level & DL_ARITH)
	    printf("__fpu_logn: 1/F=(%d,%08x,%08x...)\n", fe->fe_f2.fp_exp,
		   fe->fe_f2.fp_mant[0], fe->fe_f2.fp_mant[1]);

	/* U = (Y-F) * (1/F) */
	d = fpu_mul(fe);
	CPYFPN(&U, d);

	/* KLOG2 = K * ln(2) */
	/* fe_f1 == (fpn)k */
	fpu_explode(fe, &fe->fe_f1, FTYPE_LNG, &k);
	(void)fpu_const(&fe->fe_f2, 0x30 /* ln(2) */);
	if (fpu_debug_level & DL_ARITH) {
	    printf("__fpu_logn: fp(k)=(%d,%08x,%08x...)\n", fe->fe_f1.fp_exp,
		   fe->fe_f1.fp_mant[0], fe->fe_f1.fp_mant[1]);
	    printf("__fpu_logn: ln(2)=(%d,%08x,%08x...)\n", fe->fe_f2.fp_exp,
		   fe->fe_f2.fp_mant[0], fe->fe_f2.fp_mant[1]);
	}
	/* K * LOGOF2 */
	d = fpu_mul(fe);
	CPYFPN(&KLOG2, d);

	/* V=U*U */
	CPYFPN(&fe->fe_f1, &U);
	CPYFPN(&fe->fe_f2, &U);
	d = fpu_mul(fe);
	CPYFPN(&V, d);

	/*
	 * approximation of LOG(1+U) by
	 * (U+V*(A1+V*(A3+V*A5)))+(U*V*(A2+V*(A4+V*A6)))
	 */

	/* (U+V*(A1+V*(A3+V*A5))) part */
	CPYFPN(&fe->fe_f1, d);
	fpu_explode(fe, &fe->fe_f2, FTYPE_DBL, logA5);
	/* V*A5 */
	d = fpu_mul(fe);

	CPYFPN(&fe->fe_f1, d);
	fpu_explode(fe, &fe->fe_f2, FTYPE_DBL, logA3);
	/* A3+V*A5 */
	d = fpu_add(fe);

	CPYFPN(&fe->fe_f1, d);
	CPYFPN(&fe->fe_f2, &V);
	/* V*(A3+V*A5) */
	d = fpu_mul(fe);

	CPYFPN(&fe->fe_f1, d);
	fpu_explode(fe, &fe->fe_f2, FTYPE_DBL, logA1);
	/* A1+V*(A3+V*A5) */
	d = fpu_add(fe);

	CPYFPN(&fe->fe_f1, d);
	CPYFPN(&fe->fe_f2, &V);
	/* V*(A1+V*(A3+V*A5)) */
	d = fpu_mul(fe);

	CPYFPN(&fe->fe_f1, d);
	CPYFPN(&fe->fe_f2, &U);
	/* U+V*(A1+V*(A3+V*A5)) */
	d = fpu_add(fe);

	CPYFPN(&X, d);

	/* (U*V*(A2+V*(A4+V*A6))) part */
	CPYFPN(&fe->fe_f1, &V);
	fpu_explode(fe, &fe->fe_f2, FTYPE_DBL, logA6);
	/* V*A6 */
	d = fpu_mul(fe);
	CPYFPN(&fe->fe_f1, d);
	fpu_explode(fe, &fe->fe_f2, FTYPE_DBL, logA4);
	/* A4+V*A6 */
	d = fpu_add(fe);
	CPYFPN(&fe->fe_f1, d);
	CPYFPN(&fe->fe_f2, &V);
	/* V*(A4+V*A6) */
	d = fpu_mul(fe);
	CPYFPN(&fe->fe_f1, d);
	fpu_explode(fe, &fe->fe_f2, FTYPE_DBL, logA2);
	/* A2+V*(A4+V*A6) */
	d = fpu_add(fe);
	CPYFPN(&fe->fe_f1, d);
	CPYFPN(&fe->fe_f2, &V);
	/* V*(A2+V*(A4+V*A6)) */
	d = fpu_mul(fe);
	CPYFPN(&fe->fe_f1, d);
	CPYFPN(&fe->fe_f2, &U);
	/* U*V*(A2+V*(A4+V*A6)) */
	d = fpu_mul(fe);
	CPYFPN(&fe->fe_f1, d);
	i++;
	/* fe_f2 = logtbl[i+1] (== LOG(F)) */
	fe->fe_f2.fp_class = FPC_NUM;
	fe->fe_f2.fp_sign = fe->fe_f2.fp_sticky = fe->fe_f2.fp_mant[3] = 0;
	fe->fe_f2.fp_exp = logtbl[i].sp_exp;
	fe->fe_f2.fp_mant[0] = (logtbl[i].sp_m0 >> (31 - FP_LG));
	fe->fe_f2.fp_mant[1] = (logtbl[i].sp_m0 << (FP_LG + 1)) |
	    (logtbl[i].sp_m1 >> (31 - FP_LG));
	fe->fe_f2.fp_mant[2] = (logtbl[i].sp_m1 << (FP_LG + 1));

	if (fpu_debug_level & DL_ARITH)
	    printf("__fpu_logn: ln(F)=(%d,%08x,%08x,...)\n", fe->fe_f2.fp_exp,
		   fe->fe_f2.fp_mant[0], fe->fe_f2.fp_mant[1]);

	/* LOG(F)+U*V*(A2+V*(A4+V*A6)) */
	d = fpu_add(fe);
	CPYFPN(&fe->fe_f1, d);
	CPYFPN(&fe->fe_f2, &X);
	/* LOG(F)+U+V*(A1+V*(A3+V*A5))+U*V*(A2+V*(A4+V*A6)) */
	d = fpu_add(fe);

	if (fpu_debug_level & DL_ARITH)
	    printf("__fpu_logn: ln(Y)=(%c,%d,%08x,%08x,%08x,%08x)\n",
		   d->fp_sign ? '-' : '+', d->fp_exp,
		   d->fp_mant[0], d->fp_mant[1], d->fp_mant[2], d->fp_mant[3]);

	CPYFPN(&fe->fe_f1, d);
	CPYFPN(&fe->fe_f2, &KLOG2);
	/* K*LOGOF2+LOG(F)+U+V*(A1+V*(A3+V*A5))+U*V*(A2+V*(A4+V*A6)) */
	d = fpu_add(fe);
    }

    return d;
}

struct fpn *
fpu_log10(fe)
     struct fpemu *fe;
{
    struct fpn *fp = &fe->fe_f2;
    u_int fpsr;

    fpsr = fe->fe_fpsr & ~FPSR_EXCP;	/* clear all exceptions */

    if (fp->fp_class >= FPC_NUM) {
	if (fp->fp_sign) {	/* negative number or Inf */
	    fp = fpu_newnan(fe);
	    fpsr |= FPSR_OPERR;
	} else if (fp->fp_class == FPC_NUM) {
	    /* the real work here */
	    fp = __fpu_logn(fe);
	    if (fp != &fe->fe_f1)
		CPYFPN(&fe->fe_f1, fp);
	    (void)fpu_const(&fe->fe_f2, 0x31 /* ln(10) */);
	    fp = fpu_div(fe);
	} /* else if fp == +Inf, return +Inf */
    } else if (fp->fp_class == FPC_ZERO) {
	/* return -Inf */
	fp->fp_class = FPC_INF;
	fp->fp_sign = 1;
	fpsr |= FPSR_DZ;
    } else if (fp->fp_class == FPC_SNAN) {
	fpsr |= FPSR_SNAN;
	fp = fpu_newnan(fe);
    } else {
	fp = fpu_newnan(fe);
    }

    fe->fe_fpsr = fpsr;

    return fp;
}

struct fpn *
fpu_log2(fe)
     struct fpemu *fe;
{
    struct fpn *fp = &fe->fe_f2;
    u_int fpsr;

    fpsr = fe->fe_fpsr & ~FPSR_EXCP;	/* clear all exceptions */

    if (fp->fp_class >= FPC_NUM) {
	if (fp->fp_sign) {	/* negative number or Inf */
	    fp = fpu_newnan(fe);
	    fpsr |= FPSR_OPERR;
	} else if (fp->fp_class == FPC_NUM) {
	    /* the real work here */
	    if (fp->fp_mant[0] == FP_1 && fp->fp_mant[1] == 0 &&
		fp->fp_mant[2] == 0 && fp->fp_mant[3] == 0) {
		/* fp == 2.0 ^ exp <--> log2(fp) == exp */
		fpu_explode(fe, &fe->fe_f3, FTYPE_LNG, &fp->fp_exp);
		fp = &fe->fe_f3;
	    } else {
		fp = __fpu_logn(fe);
		if (fp != &fe->fe_f1)
		    CPYFPN(&fe->fe_f1, fp);
		(void)fpu_const(&fe->fe_f2, 0x30 /* ln(2) */);
		fp = fpu_div(fe);
	    }
	} /* else if fp == +Inf, return +Inf */
    } else if (fp->fp_class == FPC_ZERO) {
	/* return -Inf */
	fp->fp_class = FPC_INF;
	fp->fp_sign = 1;
	fpsr |= FPSR_DZ;
    } else if (fp->fp_class == FPC_SNAN) {
	fpsr |= FPSR_SNAN;
	fp = fpu_newnan(fe);
    } else {
	fp = fpu_newnan(fe);
    }

    fe->fe_fpsr = fpsr;
    return fp;
}

struct fpn *
fpu_logn(fe)
     struct fpemu *fe;
{
    struct fpn *fp = &fe->fe_f2;
    u_int fpsr;

    fpsr = fe->fe_fpsr & ~FPSR_EXCP;	/* clear all exceptions */

    if (fp->fp_class >= FPC_NUM) {
	if (fp->fp_sign) {	/* negative number or Inf */
	    fp = fpu_newnan(fe);
	    fpsr |= FPSR_OPERR;
	} else if (fp->fp_class == FPC_NUM) {
	    /* the real work here */
	    fp = __fpu_logn(fe);
	} /* else if fp == +Inf, return +Inf */
    } else if (fp->fp_class == FPC_ZERO) {
	/* return -Inf */
	fp->fp_class = FPC_INF;
	fp->fp_sign = 1;
	fpsr |= FPSR_DZ;
    } else if (fp->fp_class == FPC_SNAN) {
	fpsr |= FPSR_SNAN;
	fp = fpu_newnan(fe);
    } else {
	fp = fpu_newnan(fe);
    }

    fe->fe_fpsr = fpsr;

    return fp;
}

struct fpn *
fpu_lognp1(fe)
     struct fpemu *fe;
{
    struct fpn *fp;

    /* build a 1.0 */
    fp = fpu_const(&fe->fe_f1, 0x32); /* get 1.0 */
    /* fp = 1.0 + f2 */
    fp = fpu_add(fe);

    /* copy the result to the src opr */
    if (&fe->fe_f2 != fp)
	CPYFPN(&fe->fe_f2, fp);

    return fpu_logn(fe);
}
