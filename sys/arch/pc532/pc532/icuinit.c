/*	$NetBSD: icuinit.c,v 1.3 1994/10/26 08:25:03 cgd Exp $	*/

/* 
 * Copyright (c) 1993 Philip A. Nelson.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Philip A. Nelson.
 * 4. The name of Philip A. Nelson may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PHILIP NELSON ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL PHILIP NELSON BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* icuinit.c - C support for 532 icu stuff. */

#include <sys/param.h>
#include <machine/cpu.h>
#include <machine/icu.h>

/* Table for initializing ICU.  Written in order to ICU.
 * Monitor has already set
 *   CCTL=0x15 MCTL=0 LCSV=13 IPS=0 PDIR=0xfe PDAT=0xfe
 */
struct icu_init_type {
  unsigned char offset;
  unsigned char val;
} icu_tab [] = {
  {IMSK,	0xff},			/* mask off ints for now */
  {IMSK+1,	0xff},			/*   "   */
  {CIPTR,	IR_CLK << 4},		/* clock int vector (CIPTR) */
  {CICTL,	0x00},			/* clock interrupt enable (CICTL) */
  {IPS,		0x7e},			/* 0=i/o, 1=int_req */
  {PDIR,	0x7e},			/* 1=in, 0=out */
  {OCASN,	0},			/* clock output (n/a) */
  {PDAT,	0xfe},			/* keep ROM at high mem */
  {ISRV,	0},			/* int in service (set by hw) */
  {ISRV+1,	0},			/* int in service */
  {CSRC,	0},			/* cascade source */
  {CSRC+1,	0},			/* cascade source */
  {SVCT,	VEC_ICU},		/* isr vector */
  {FPRT,	0},			/* priority */
  {TPL,		IPOLARITY & 0xff},	/* polarity */
  {TPL+1,	IPOLARITY >> 8},	/* " */
  {ELTG,	IEDGE & 0xff},		/* edge trigger (0 = edge) */
  {ELTG+1,	IEDGE >> 8},		/* " */
  {MCTL,	0x02},			/* fixed priority */
  {CCTL,	0x1c},			/* prescale, no cat, run clocks */
};
#define ICUTAB_SZ (sizeof (icu_tab) / sizeof (struct icu_init_type))


/*===========================================================================*
 *				icu_init
 *===========================================================================*/

/* Initialize interrupt control unit.  Starts clock interrupts.
 */

icu_init()
{
  struct icu_init_type *p;

  for (p = icu_tab; p < icu_tab + ICUTAB_SZ; ++p)
    WR_ADR (unsigned char, ICU_ADR + p->offset, p->val);
}

