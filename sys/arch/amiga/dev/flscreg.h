/*
 * Copyright (c) 1995 Daniel Widenfalk
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
 *      This product includes software developed by Daniel Widenfalk
 *      for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * FastlaneZ3 with FAS216 SCSI interface hardware description.
 */

#ifndef _FLSCREG_H_
#define _FLSCREG_H_

typedef struct flsc_regmap {
	sfas_regmap_t	FAS216;
	vu_char		*hardbits;
	vu_char		*clear;
	vu_char		*dmabase;
} flsc_regmap_t;
typedef flsc_regmap_t *flsc_regmap_p;

#define FLSC_HB_DISABLED	0x01
#define FLSC_HB_BUSID6		0x02
#define FLSC_HB_SEAGATE		0x04
#define FLSC_HB_SLOW		0x08
#define FLSC_HB_SYNCHRON	0x10
#define FLSC_HB_CREQ		0x20
#define FLSC_HB_IACT		0x40
#define FLSC_HB_MINT		0x80

#define FLSC_PB_ESI		0x01
#define FLSC_PB_EDI		0x02
#define FLSC_PB_ENABLE_DMA	0x04
#define FLSC_PB_DISABLE_DMA	0x00	/* Symmetric reasons */
#define FLSC_PB_DMA_WRITE	0x08
#define FLSC_PB_DMA_READ	0x00	/* Symmetric reasons */
#define FLSC_PB_LED		0x10

#define FLSC_PB_INT_BITS (FLSC_PB_ESI | FLSC_PB_EDI)
#define FLSC_PB_DMA_BITS (FLSC_PB_ENABLE_DMA | FLSC_PB_DMA_WRITE)

#endif
