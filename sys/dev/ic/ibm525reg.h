/*	$OpenBSD: ibm525reg.h,v 1.2 2010/08/02 20:55:49 kettenis Exp $	*/
/*
 * Copyright (c) 2005, Miodrag Vallat.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * IBM RGB525 Ramdac registers
 */

#define	IBM525_REVISION		0x00
#define	IBM525_ID		0x01

/* Miscellaneous clock control */
#define	IBM525_MISC_CLOCK	0x02
#define	MC_B24P_PLL		0x00
#define	MC_B24P_SCLK		0x20
#define	MC_DDOT_DIV_1		0x00	/* (VRAM size / 1) bpp */
#define	MC_DDOT_DIV_2		0x02	/* (VRAM size / 2) bpp */
#define	MC_DDOT_DIV_4		0x04	/* (VRAM size / 4) bpp */
#define	MC_DDOT_DIV_8		0x06	/* (VRAM size / 8) bpp */
#define	MC_DDOT_DIV_16		0x08	/* (VRAM size / 16) bpp */
#define	MC_DDOT_DIV_MASK	0x0e
#define	MC_PLL_ENABLE		0x01

/* Sync control */
#define	IBM525_SYNC		0x03
#define	S_CSYN_INVERT		0x40	/* Composite sync invert */
#define	S_VSYN_INVERT		0x20	/* Vertical sync invert (positive) */
#define	S_HSYN_INVERT		0x10	/* Horizontal sync invert (positive) */
#define	S_VSYN_NORMAL		0x00
#define	S_VSYN_HIGH		0x04
#define	S_VSYN_LOW		0x08
#define	S_VSYN_DISABLE		0x0c
#define	S_VSYN_MASK		0x0c
#define	S_HSYN_NORMAL		0x00
#define	S_HSYN_HIGH		0x01
#define	S_HSYN_LOW		0x02
#define	S_HSYN_DISABLE		0x03
#define	S_HSYN_MASK		0x03

/* Horizontal sync position */
#define	IBM525_HSYNC_POS	0x04

/* Power management */
#define	IBM525_POWER		0x05
#define	P_SCLK_DISABLE		0x10
#define	P_DDOT_DISABLE		0x08
#define	P_SYNC_DISABLE		0x04
#define	P_ICLK_DISABLE		0x02	/* Disable internal DAC clock */
#define	P_DAC_PWR_DISABLE	0x01	/* Disable internal DAC power */

/* DAC operation */
#define	IBM525_DAC_OP		0x06
#define	DO_SOG			0x08	/* Sync on Green */
#define	DO_FAST_SLEW		0x02	/* fast (>= 20 MHz) pixel clock */
#define	DO_BLANK_BR		0x04	/* blank blue and red channels */
#define	DO_PEDESTAL		0x01

/* Palette control */
#define	IBM525_PALETTE		0x07

/* System clock control */
#define	IBM525_SYSCLK		0x08
#define	SC_ENABLE		0x01

/* Pixel format */
#define	IBM525_PIXEL		0x0a
#define	PIX_4BPP		0x02
#define	PIX_8BPP		0x03
#define	PIX_16BPP		0x04
#define	PIX_24BPP		0x05
#define	PIX_32BPP		0x06

/* 8bpp pixel format */
#define	IBM525_PF8		0x0b
#define	PF8_INDIRECT		0x00
#define	PF8_DIRECT		0x01

/* 16bpp pixel format */
#define	IBM525_PF16		0x0c
#define	PF16_INDIRECT		0x00
#define	PF16_DIRECT		0xc0
#define	PF16_LINEAR		0x04
#define	PF16_555		0x00	/* 5:5:5 15bpp */
#define	PF16_565		0x02	/* 5:6:5 15bpp */

/* 24bpp pixel format */
#define	IBM525_PF24		0x0d
#define	PF24_INDIRECT		0x00
#define	PF24_DIRECT		0x01

/* 32bpp pixel format */
#define	IBM525_PF32		0x0e
#define	PF32_INDIRECT		0x00
#define	PF32_DIRECT		0x03
#define	PF32_BYPASS		0x00
#define	PF32_LOOKUP		0x04

/* Pixel PLL control #1 */
#define	IBM525_PLL1		0x10
#define	P1_CLK_REF		0x00
#define	P1_CLK_EXT		0x10
#define	P1_SRC_EXT_F		0x00	/* Use F registers for timing */
#define	P1_SRC_EXT_MN		0x01	/* Use M:N registers for timing */
#define	P1_SRC_DIRECT_F		0x02	/* Use F registers for timing */
#define	P1_SRC_DIRECT_MN	0x03	/* Use M:N registers for timing */

/* Pixel PLL control #2 */
#define	IBM525_PLL2		0x11

/* Fixed PLL reference */
#define	IBM525_PLL_FIXED_REF	0x14

/* PLL reference divider */
#define	IBM525_PLL_REF_DIV	0x15

/* PLL VCO divider */
#define	IBM525_PLL_VCO_DIV	0x16

/* N0-N15 */
#define	IBM525_F(n)		(0x20 + (n))

/* Miscellaneous control #1 */
#define	IBM525_MISC1		0x70
#define	M1_VRAM_32		0x00
#define	M1_VRAM_64		0x01
#define	M1_VRAM_SIZE_MASK	0x01
#define	M1_SENSE_DISABLE	0x10

/* Miscellaneous control #2 */
#define	IBM525_MISC2		0x71
#define	M2_PCLK_LOCAL		0x00
#define	M2_PCLK_PLL		0x40
#define	M2_PCLK_EXT		0x80
#define	M2_PCLK_MASK		0xc0
#define	M2_PALETTE_6		0x00	/* VGA compatible 6bit palette */
#define	M2_PALETTE_8		0x04	/* 8bit palette */
#define	M2_PALETTE_MASK		0x04
#define	M2_MODE_VRAM		0x01
#define	M2_MODE_VGA		0x00
#define	M2_MODE_MASK		0x01

/* Miscellaneous control #3 */
#define	IBM525_MISC3		0x72
#define	M3_SWAP_BR		0x80	/* swap blue and red */
#define	M3_SWAP_WORDS		0x10
#define	M3_SWAP_NIBBLES		0x02

/* Miscellaneous control #4 */
#define	IBM525_MISC4		0x73
#define	M4_INVERT_DCLK		0x10
#define	M4_FAST			0x20	/* Fast (>= 50 MHz) pixel clock */

/*
 * Pixel clock encoding
 */
#define	MHZ_TO_PLL(m) \
	((m) < 32 ? 0x00 | (4 * (m) - 65) : \
	   (m) < 64 ? 0x40 | (2 * (m) - 65) : \
	     (m) < 128 ? 0x80 | ((m) - 65) : \
	       0xc0 | ((m) / 2 - 65))
