/* $OpenBSD: s3c24x0reg.h,v 1.2 2009/01/05 20:37:16 jasper Exp $ */
/* $NetBSD: s3c24x0reg.h,v 1.8 2005/12/11 12:16:51 christos Exp $ */

/*
 * Copyright (c) 2003  Genetec corporation  All rights reserved.
 * Written by Hiroyuki Bessho for Genetec corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Genetec corporation may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GENETEC CORP. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GENETEC CORP.
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


/*
 * Samsung S3C2410X/2400 processor is ARM920T based integrated CPU
 *
 * Reference:
 *  S3C2410X User's Manual 
 *  S3C2400 User's Manual
 */
#ifndef _ARM_S3C2XX0_S3C24X0REG_H_
#define	_ARM_S3C2XX0_S3C24X0REG_H_

/* Memory controller */
#define	MEMCTL_BWSCON   	0x00	/* Bus width and wait status */
#define	 BWSCON_DW0_SHIFT	1 	/* bank0 is odd */
#define	 BWSCON_BANK_SHIFT(n)	(4*(n))	/* for bank 1..7 */
#define	 BWSCON_DW_MASK 	0x03
#define	 BWSCON_DW_8 		0
#define	 BWSCON_DW_16 		1
#define	 BWSCON_DW_32 		2
#define	 BWSCON_WS		0x04	/* WAIT enable for the bank */
#define	 BWSCON_ST		0x08	/* SRAM use UB/LB for the bank */

#define	MEMCTL_BANKCON0 	0x04	/* Boot ROM control */
#define	MEMCTL_BANKCON(n)	(0x04+4*(n)) /* BANKn control */
#define	 BANKCON_MT_SHIFT 	15
#define	 BANKCON_MT_ROM 	(0<<BANKCON_MT_SHIFT)
#define	 BANKCON_MT_DRAM 	(3<<BANKCON_MT_SHIFT)
#define	 BANKCON_TACS_SHIFT 	13	/* address set-up time to nGCS */
#define	 BANKCON_TCOS_SHIFT 	11	/* CS set-up to nOE */
#define	 BANKCON_TACC_SHIFT 	8	/* CS set-up to nOE */
#define	 BANKCON_TOCH_SHIFT 	6	/* CS hold time from OE */
#define	 BANKCON_TCAH_SHIFT 	4	/* address hold time from OE */
#define	 BANKCON_TACP_SHIFT 	2	/* page mode access cycle */
#define	 BANKCON_TACP_2 	(0<<BANKCON_TACP_SHIFT)
#define	 BANKCON_TACP_3  	(1<<BANKCON_TACP_SHIFT)
#define	 BANKCON_TACP_4  	(2<<BANKCON_TACP_SHIFT)
#define	 BANKCON_TACP_6  	(3<<BANKCON_TACP_SHIFT)
#define	 BANKCON_PMC_4   	(1<<0)
#define	 BANKCON_PMC_8   	(2<<0)
#define	 BANKCON_PMC_16   	(3<<0)
#define	 BANKCON_TRCD_SHIFT 	2	/* RAS to CAS delay */
#define	 BANKCON_TRCD_2  	(0<<2)
#define	 BANKCON_TRCD_3  	(1<<2)
#define	 BANKCON_TRCD_4  	(2<<2)
#define	 BANKCON_SCAN_8 	(0<<0)	/* Column address number */
#define	 BANKCON_SCAN_9 	(1<<0)
#define	 BANKCON_SCAN_10 	(2<<0)
#define	MEMCTL_REFRESH   	0x24	/* DRAM?SDRAM Refresh */
#define	 REFRESH_REFEN 		(1<<23)
#define	 REFRESH_TREFMD  	(1<<22)	/* 1=self refresh */
#define	 REFRESH_TRP_2 		(0<<20)
#define	 REFRESH_TRP_3 		(1<<20)
#define	 REFRESH_TRP_4 		(2<<20)
#define	 REFRESH_TRC_4 		(0<<18)
#define	 REFRESH_TRC_5 		(1<<18)
#define	 REFRESH_TRC_6 		(2<<18)
#define	 REFRESH_TRC_7 		(3<<18)
#define	 REFRESH_COUNTER_MASK	0x3ff
#define	MEMCTL_BANKSIZE 	0x28 	/* Flexible Bank size */
#define	MEMCTL_MRSRB6    	0x2c	/* SDRAM Mode register */
#define	MEMCTL_MRSRB7    	0x30
#define	 MRSR_CL_SHIFT		4	/* CAS Latency */

#define	S3C24X0_MEMCTL_SIZE	0x34

/* USB Host controller */
#define	S3C24X0_USBHC_SIZE	0x5c

/* Interrupt controller */
#define	INTCTL_PRIORITY 	0x0c	/* IRQ Priority control */
#define	INTCTL_INTPND   	0x10	/* Interrupt request status */
#define	INTCTL_INTOFFSET	0x14	/* Interrupt request source */

/* Interrupt source */
#define	S3C24X0_INT_ADCTC 	31	/* ADC (and TC for 2410 */
#define	S3C24X0_INT_RTC  	30	/* RTC alarm */
#define	S3C2400_INT_UTXD1	29	/* UART1 Tx INT  (2400 only) */
#define	S3C2410_INT_SPI1	29	/* SPI 1 (2410 only) */
#define	S3C2400_INT_UTXD0	28	/* UART0 Tx INT  (2400 only) */
#define	S3C2410_INT_UART0	28	/* UART0 (2410 only) */
#define	S3C24X0_INT_IIC  	27
#define	S3C24X0_INT_USBH	26	/* USB Host */
#define	S3C24X0_INT_USBD	25	/* USB Device */
#define	S3C2400_INT_URXD1	24	/* UART1 Rx INT (2400 only) */
#define	S3C2400_INT_URXD0	23	/* UART0 Rx INT (2400 only) */
#define	S3C2410_INT_UART1	23	/* UART0  (2410 only) */
#define	S3C24X0_INT_SPI0  	22	/* SPI 0 */
#define	S3C2400_INT_MMC 	21
#define	S3C2410_INT_SDI 	21
#define	S3C24X0_INT_DMA3	20
#define	S3C24X0_INT_DMA2	19
#define	S3C24X0_INT_DMA1	18
#define	S3C24X0_INT_DMA0	17
#define	S3C2410_INT_LCD 	16

#define	S3C2400_INT_UERR 	15	/* UART 0/1 Error int (2400) */
#define	S3C2410_INT_UART2 	15	/* UART2 int (2410) */
#define	S3C24X0_INT_TIMER4	14
#define	S3C24X0_INT_TIMER3	13
#define	S3C24X0_INT_TIMER2	12
#define	S3C24X0_INT_TIMER1	11
#define	S3C24X0_INT_TIMER0	10
#define	S3C24X0_INT_TIMER(n)	(10+(n)) /* timer interrupt [4:0] */
#define	S3C24X0_INT_WDT 	9	/* Watch dog timer */
#define	S3C24X0_INT_TICK 	8
#define	S3C2410_INT_BFLT 	7	/* Battery fault */
#define	S3C2410_INT_8_23	5	/* Ext int 8..23 */
#define	S3C2410_INT_4_7 	4	/* Ext int 4..7 */
#define	S3C24X0_INT_EXT(n)	(n) 	/* External interrupt [7:0] for 2400,
					 * [3:0] for 2410 */
/* DMA controller */
/* XXX */

/* Clock & power manager */
#define	CLKMAN_LOCKTIME 0x00	/* PLL lock time */
#define	CLKMAN_MPLLCON 	0x04	/* MPLL control */
#define	CLKMAN_UPLLCON 	0x08	/* UPLL control */
#define	 PLLCON_MDIV_SHIFT	12
#define	 PLLCON_MDIV_MASK	(0xff<<PLLCON_MDIV_SHIFT)
#define	 PLLCON_PDIV_SHIFT	4
#define	 PLLCON_PDIV_MASK	(0x3f<<PLLCON_PDIV_SHIFT)
#define	 PLLCON_SDIV_SHIFT	0
#define	 PLLCON_SDIV_MASK	(0x03<<PLLCON_SDIV_SHIFT)
#define	CLKMAN_CLKCON	0x0c

#define	CLKMAN_CLKSLOW	0x10	/* slow clock controll */
#define	 CLKSLOW_UCLK 	(1<<7)	/* 1=UPLL off */
#define	 CLKSLOW_MPLL 	(1<<5)	/* 1=PLL off */
#define	 CLKSLOW_SLOW	(1<<4)	/* 1: Enable SLOW mode */
#define	 CLKSLOW_VAL_MASK  0x0f	/* divider value for slow clock */

#define	CLKMAN_CLKDIVN	0x14	/* Software reset control */
#define	 CLKDIVN_HDIVN	(1<<1)
#define	 CLKDIVN_PDIVN	(1<<0)

#define	S3C24X0_CLKMAN_SIZE	0x18

/* LCD controller */
#define	LCDC_LCDCON1	0x00	/* control 1 */
#define	 LCDCON1_ENVID   	(1<<0)	/* enable video */
#define	 LCDCON1_BPPMODE_SHIFT 	1
#define	 LCDCON1_BPPMODE_MASK	(0x0f<<LCDCON1_BPPMODE_SHIFT)
#define	 LCDCON1_BPPMODE_STN1	(0x0<<LCDCON1_BPPMODE_SHIFT)
#define	 LCDCON1_BPPMODE_STN2	(0x1<<LCDCON1_BPPMODE_SHIFT)
#define	 LCDCON1_BPPMODE_STN4	(0x2<<LCDCON1_BPPMODE_SHIFT)
#define	 LCDCON1_BPPMODE_STN8	(0x3<<LCDCON1_BPPMODE_SHIFT)
#define	 LCDCON1_BPPMODE_STN12	(0x4<<LCDCON1_BPPMODE_SHIFT)
#define	 LCDCON1_BPPMODE_TFT1	(0x8<<LCDCON1_BPPMODE_SHIFT)
#define	 LCDCON1_BPPMODE_TFT2	(0x9<<LCDCON1_BPPMODE_SHIFT)
#define	 LCDCON1_BPPMODE_TFT4	(0xa<<LCDCON1_BPPMODE_SHIFT)
#define	 LCDCON1_BPPMODE_TFT8	(0xb<<LCDCON1_BPPMODE_SHIFT)
#define	 LCDCON1_BPPMODE_TFT16	(0xc<<LCDCON1_BPPMODE_SHIFT)
#define	 LCDCON1_BPPMODE_TFT24	(0xd<<LCDCON1_BPPMODE_SHIFT)
#define	 LCDCON1_BPPMODE_TFTX	(0x8<<LCDCON1_BPPMODE_SHIFT)

#define	 LCDCON1_PNRMODE_SHIFT	5
#define	 LCDCON1_PNRMODE_MASK	(0x3<<LCDCON1_PNRMODE_SHIFT)
#define	 LCDCON1_PNRMODE_DUALSTN4    (0x0<<LCDCON1_PNRMODE_SHIFT)
#define	 LCDCON1_PNRMODE_SINGLESTN4  (0x1<<LCDCON1_PNRMODE_SHIFT)
#define	 LCDCON1_PNRMODE_SINGLESTN8  (0x2<<LCDCON1_PNRMODE_SHIFT)
#define	 LCDCON1_PNRMODE_TFT         (0x3<<LCDCON1_PNRMODE_SHIFT)

#define	 LCDCON1_MMODE  	(1<<7) /* VM toggle rate */
#define	 LCDCON1_CLKVAL_SHIFT 	8
#define	 LCDCON1_CLKVAL_MASK	(0x3ff<<LCDCON1_CLKVAL_SHIFT)
#define	 LCDCON1_LINCNT_SHIFT 	18
#define	 LCDCON1_LINCNT_MASK	(0x3ff<<LCDCON1_LINCNT_SHIFT)

#define	LCDC_LCDCON2	0x04	/* control 2 */
#define	 LCDCON2_VPSW_SHIFT	0 	/* TFT Vsync pulse width */
#define	 LCDCON2_VPSW_MASK	(0x3f<<LCDCON2_VPSW_SHIFT)
#define	 LCDCON2_VFPD_SHIFT	6 	/* TFT V front porch */
#define	 LCDCON2_VFPD_MASK	(0xff<<LCDCON2_VFPD_SHIFT)
#define	 LCDCON2_LINEVAL_SHIFT	14 	/* Vertical size */
#define	 LCDCON2_LINEVAL_MASK	(0x3ff<<LCDCON2_LINEVAL_SHIFT)
#define	 LCDCON2_VBPD_SHIFT	24 	/* TFT V back porch */
#define	 LCDCON2_VBPD_MASK	(0xff<<LCDCON2_VBPD_SHIFT)

#define	LCDC_LCDCON3	0x08	/* control 2 */
#define	 LCDCON3_HFPD_SHIFT	0 	/* TFT H front porch */
#define	 LCDCON3_HFPD_MASK	(0xff<<LCDCON3_VPFD_SHIFT)
#define	 LCDCON3_LINEBLANK_SHIFT  0 	/* STN H blank time */
#define	 LCDCON3_LINEBLANK_MASK	  (0xff<<LCDCON3_LINEBLANK_SHIFT)
#define	 LCDCON3_HOZVAL_SHIFT	8 	/* Horizontal size */
#define	 LCDCON3_HOZVAL_MASK	(0x7ff<<LCDCON3_HOZVAL_SHIFT)
#define	 LCDCON3_HBPD_SHIFT	19 	/* TFT H back porch */
#define	 LCDCON3_HBPD_MASK	(0x7f<<LCDCON3_HPBD_SHIFT)
#define	 LCDCON3_WDLY_SHIFT	19	/* STN vline delay */
#define	 LCDCON3_WDLY_MASK	(0x03<<LCDCON3_WDLY_SHIFT)
#define	 LCDCON3_WDLY_16	(0x00<<LCDCON3_WDLY_SHIFT)
#define	 LCDCON3_WDLY_32	(0x01<<LCDCON3_WDLY_SHIFT)
#define	 LCDCON3_WDLY_64	(0x02<<LCDCON3_WDLY_SHIFT)
#define	 LCDCON3_WDLY_128	(0x03<<LCDCON3_WDLY_SHIFT)

#define	LCDC_LCDCON4	0x0c	/* control 4 */
#define	 LCDCON4_HPSW_SHIFT	0 	/* TFT Hsync pulse width */
#define	 LCDCON4_HPSW_MASK	(0xff<<LCDCON4_HPSW_SHIFT)
#define	 LCDCON4_WLH_SHIFT	0	/* STN VLINE high width */
#define	 LCDCON4_WLH_MASK	(0x03<<LCDCON4_WLH_SHIFT)
#define	 LCDCON4_WLH_16 	(0x00<<LCDCON4_WLH_SHIFT)
#define	 LCDCON4_WLH_32  	(0x01<<LCDCON4_WLH_SHIFT)
#define	 LCDCON4_WLH_64  	(0x02<<LCDCON4_WLH_SHIFT)
#define	 LCDCON4_WLH_128	(0x03<<LCDCON4_WLH_SHIFT)

#define	 LCDCON4_MVAL_SHIFT	8	/* STN VM toggle rate */
#define	 LCDCON4_MVAL_MASK	(0xff<<LCDCON4_MVAL_SHIFT)

#define	LCDC_LCDCON5	0x10	/* control 5 */
#define	 LCDCON5_HWSWP		(1<<0)	/* half-word swap */
#define	 LCDCON5_BSWP 		(1<<1)	/* byte swap */
#define	 LCDCON5_ENLEND		(1<<2)	/* TFT: enable LEND signal */
#define	 LCDCON5_PWREN		(1<<3)	/* enable PWREN signale */
#define	 LCDCON5_INVLEND	(1<<4)	/* TFT: LEND signal polarity */
#define	 LCDCON5_INVPWREN	(1<<5)	/* PWREN signal polarity */
#define	 LCDCON5_INVVDEN	(1<<6)	/* VDEN signal polarity */
#define	 LCDCON5_INVVD		(1<<7)	/* video data signal polarity */
#define	 LCDCON5_INVVFRAME	(1<<8)	/* VFRAME/VSYNC signal polarity */
#define	 LCDCON5_INVVLINE	(1<<9)	/* VLINE/HSYNC signal polarity */
#define	 LCDCON5_INVVCLK	(1<<10)	/* VCLK signal polarity */
#define	 LCDCON5_INVVCLK_RISING	LCDCON5_INVVCLK
#define	 LCDCON5_INVVCLK_FALLING  0
#define	 LCDCON5_FRM565  	(1<<11)	/* RGB:565 format*/
#define	 LCDCON5_FRM555I	0	/* RGBI:5551 format */
#define	 LCDCON5_BPP24BL	(1<<12)	/* bit order for bpp24 */

#define	 LCDCON5_HSTATUS_SHIFT	17 /* TFT: horizontal status */
#define	 LCDCON5_HSTATUS_MASK	(0x03<<LCDCON5_HSTATUS_SHIFT)
#define	 LCDCON5_HSTATUS_HSYNC	(0x00<<LCDCON5_HSTATUS_SHIFT)
#define	 LCDCON5_HSTATUS_BACKP	(0x01<<LCDCON5_HSTATUS_SHIFT)
#define	 LCDCON5_HSTATUS_ACTIVE	(0x02<<LCDCON5_HSTATUS_SHIFT)
#define	 LCDCON5_HSTATUS_FRONTP	(0x03<<LCDCON5_HSTATUS_SHIFT)

#define	 LCDCON5_VSTATUS_SHIFT	19 /* TFT: vertical status */
#define	 LCDCON5_VSTATUS_MASK	(0x03<<LCDCON5_VSTATUS_SHIFT)
#define	 LCDCON5_VSTATUS_HSYNC	(0x00<<LCDCON5_VSTATUS_SHIFT)
#define	 LCDCON5_VSTATUS_BACKP	(0x01<<LCDCON5_VSTATUS_SHIFT)
#define	 LCDCON5_VSTATUS_ACTIVE	(0x02<<LCDCON5_VSTATUS_SHIFT)
#define	 LCDCON5_VSTATUS_FRONTP	(0x03<<LCDCON5_VSTATUS_SHIFT)

#define	LCDC_LCDSADDR1	0x14	/* frame buffer start address */
#define	LCDC_LCDSADDR2	0x18
#define	LCDC_LCDSADDR3	0x1c
#define	 LCDSADDR3_OFFSIZE_SHIFT     11
#define	 LCDSADDR3_PAGEWIDTH_SHIFT   0

#define	LCDC_REDLUT	0x20	/* STN: red lookup table */
#define	LCDC_GREENLUT	0x24	/* STN: green lookup table */
#define	LCDC_BLUELUT	0x28	/* STN: blue lookup table */
#define	LCDC_DITHMODE	0x4c	/* STN: dithering mode */

#define	LCDC_TPAL	0x50	/* TFT: temporary palette */
#define	 TPAL_TPALEN		(1<<24)
#define	 TPAL_RED_SHIFT  	16
#define	 TPAL_GREEN_SHIFT	8
#define	 TPAL_BLUE_SHIFT 	0

#define	LCDC_LCDINTPND	0x54
#define	LCDC_LCDSRCPND	0x58
#define	LCDC_LCDINTMSK	0x5c
#define	 LCDINT_FICNT	(1<<0)	/* FIFO trigger interrupt pending */
#define	 LCDINT_FRSYN	(1<<1)	/* frame sync interrupt pending */
#define	 LCDINT_FIWSEL	(1<<2)	/* FIFO trigger level: 1=8 words, 0=4 words*/

#define	LCDC_LPCSEL	0x60	/* LPC3600 mode  */
#define	 LPCSEL_LPC_EN		(1<<0)	/* enable LPC3600 mode */
#define	 LPCSEL_RES_SEL		(1<<1)	/* 1=240x320 0=320x240 */
#define	 LPCSEL_MODE_SEL	(1<<2)
#define	 LPCSEL_CPV_SEL		(1<<3)


#define	LCDC_PALETTE		0x0400
#define	LCDC_PALETTE_SIZE	0x0400

#define	S3C24X0_LCDC_SIZE 	(LCDC_PALETTE+LCDC_PALETTE_SIZE)

/* Timer */
#define	TIMER_TCFG0 	0x00	/* Timer configuration */
#define	TIMER_TCFG1	0x04
#define	 TCFG1_MUX_SHIFT(n)	(4*(n))
#define	 TCFG1_MUX_MASK(n)	(0x0f << TCFG1_MUX_SHIFT(n))
#define	 TCFG1_MUX_DIV2		0
#define	 TCFG1_MUX_DIV4		1
#define	 TCFG1_MUX_DIV8		2
#define	 TCFG1_MUX_DIV16	3
#define	 TCFG1_MUX_EXT 		4
#define	TIMER_TCON 	0x08	/* control */
#define	 TCON_SHIFT(n)		(4 * ((n)==0 ? 0 : (n)+1))
#define	 TCON_START(n)		(1 << TCON_SHIFT(n))
#define	 TCON_MANUALUPDATE(n)	(1 << (TCON_SHIFT(n) + 1))
#define	 TCON_INVERTER(n)	(1 << (TCON_SHIFT(n) + 2))
#define	 __TCON_AUTORELOAD(n)	(1 << (TCON_SHIFT(n) + 3)) /* n=0..3 */
#define	 TCON_AUTORELOAD4 	(1<<22)	       /* stupid hardware design */
#define	 TCON_AUTORELOAD(n)	((n)==4 ? TCON_AUTORELOAD4 : __TCON_AUTORELOAD(n))
#define	 TCON_MASK(n)		(0x0f << TCON_SHIFT(n))
#define	TIMER_TCNTB(n) 	 (0x0c+0x0c*(n))	/* count buffer */
#define	TIMER_TCMPB(n)	 (0x10+0x0c*(n))	/* compare buffer */
#define	__TIMER_TCNTO(n) (0x14+0x0c*(n))	/* count observation */
#define	TIMER_TCNTO4	0x40
#define	TIMER_TCNTO(n)	((n)==4 ? TIMER_TCNTO4 : __TIMER_TCNTO(n))

#define	S3C24X0_TIMER_SIZE	0x44

/* UART */
/* diffs to s3c2800 */
#define	 UMCON_AFC	(1<<4)	/* auto flow control */
#define	 UMSTAT_DCTS	(1<<2)	/* CTS change */
#define	 ULCON_IR  	(1<<6)
#define	 ULCON_PARITY_SHIFT  3

#define	S3C24X0_UART_SIZE 	0x2c

/* USB device */
/* XXX */

/* Watch dog timer */
#define	WDT_WTCON 	0x00	/* WDT mode */
#define	 WTCON_PRESCALE_SHIFT	8
#define	 WTCON_PRESCALE	(0xff<<WTCON_PRESCALE_SHIFT)
#define	 WTCON_ENABLE   (1<<5)
#define	 WTCON_CLKSEL	(3<<3)
#define	 WTCON_CLKSEL_16  (0<<3)
#define	 WTCON_CLKSEL_32  (1<<3)
#define	 WTCON_CLKSEL_64  (2<<3)
#define	 WTCON_CLKSEL_128 (3<<3)
#define	 WTCON_ENINT    (1<<2)
#define	 WTCON_ENRST	(1<<0)

#define	 WTCON_WDTSTOP	0
	
#define	WDT_WTDAT 	0x04	/* timer data */
#define	WDT_WTCNT 	0x08	/* timer count */

#define	S3C24X0_WDT_SIZE 	0x0c

/* IIC */ /* XXX */
#define	S3C24X0_IIC_SIZE 	0x0c


/* IIS */ /* XXX */
#define	S3C24X0_IIS_SIZE 	0x14

/* RTC */ /* XXX */

/* SPI */
#define	S3C24X0_SPI_SIZE 	0x20

#define	SPI_SPCON		0x00
#define	 SPCON_TAGD		(1<<0) /* Tx auto garbage */
#define	 SPCON_CPHA		(1<<1)
#define	 SPCON_CPOL		(1<<2)
#define	 SPCON_IDLELOW_RISING	  (0|0)
#define	 SPCON_IDLELOW_FALLING	  (0|SPCON_CPHA)
#define	 SPCON_IDLEHIGH_FALLING  (SPCON_CPOL|0) 
#define	 SPCON_IDLEHIGH_RISING	  (SPCON_CPOL|SPCON_CPHA)
#define	 SPCON_MSTR		(1<<3)
#define	 SPCON_ENSCK		(1<<4)
#define	 SPCON_SMOD_SHIFT	5
#define	 SPCON_SMOD_MASK	(0x03<<SPCON_SMOD_SHIFT)
#define	 SPCON_SMOD_POLL	(0x00<<SPCON_SMOD_SHIFT)
#define	 SPCON_SMOD_INT 	(0x01<<SPCON_SMOD_SHIFT)
#define	 SPCON_SMOD_DMA 	(0x02<<SPCON_SMOD_SHIFT)

#define	SPI_SPSTA		0x04 /* status register */
#define	 SPSTA_REDY		(1<<0) /* ready */
#define	 SPSTA_MULF		(1<<1) /* multi master error */
#define	 SPSTA_DCOL		(1<<2) /* Data collision error */

#define	SPI_SPPIN		0x08
#define	 SPPIN_KEEP		(1<<0)
#define	 SPPIN_ENMUL		(1<<2) /* multi master error detect */

#define	SPI_SPPRE		0x0c /* prescaler */
#define	SPI_SPTDAT		0x10 /* tx data */
#define	SPI_SPRDAT		0x14 /* rx data */


#endif /* _ARM_S3C2XX0_S3C24X0REG_H_ */
