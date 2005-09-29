/*	$OpenBSD: viasioreg.h,v 1.2 2005/09/29 19:53:24 grange Exp $	*/
/*
 * Copyright (c) 2005 Alexander Yurchenko <grange@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _DEV_ISA_VIASIOREG_H_
#define _DEV_ISA_VIASIOREG_H_

/*
 * VIA VT1211 LPC Super I/O register definitions.
 */

/*
 * Obtained from the following datasheet:
 *
 * VT1211
 * Low Pin Count Super I/O And Hardware Monitor
 * Revision 1.0
 * January 8, 2002
 * VIA TECHNOLOGIES, INC.
 */

/* ISA bus registers */
#define VT1211_INDEX		0x00	/* Configuration Index Register */
#define VT1211_DATA		0x01	/* Configuration Data Register */

#define VT1211_IOSIZE		0x02	/* ISA I/O space size */

#define VT1211_CONF_EN_MAGIC	0x87	/* enable configuration mode */
#define VT1211_CONF_DS_MAGIC	0xaa	/* disable configuration mode */

/* Configuration Space Registers */
#define VT1211_LDN		0x07	/* Logical Device Number */
#define VT1211_ID		0x20	/* Device ID */
#define VT1211_REV		0x21	/* Device Revision */
#define VT1211_PDC		0x22	/* Power Down Control */
#define VT1211_LPCWSS		0x23	/* LPC Wait State Select */
#define VT1211_GPIO1PS		0x24	/* GPIO Port 1 Pin Select */
#define VT1211_GPIO2PS		0x25	/* GPIO Port 2 Pin Select */
#define VT1211_GPIO7PS		0x26	/* GPIO Port 7 Pin Select */
#define VT1211_UART2PS		0x27	/* UART2 Multi Function Pin Select */
#define VT1211_MIDIPS		0x28	/* MIDI Multi Function Pin Select */
#define VT1211_HWMPS		0x29	/* HWM Multi Function Pin Select */
#define VT1211_TMA		0x2e	/* Test Mode A */
#define VT1211_TMB		0x2f	/* Test Mode B */

#define VT1211_ID_VT1211	0x3c	/* VT1211 Device ID */

/* Logical Device Number (LDN) Assignments */
#define VT1211_LDN_FDC		0x00	/* Floppy Disk Controller */
#define VT1211_LDN_PP		0x01	/* Parallel Port */
#define VT1211_LDN_UART1	0x02	/* Serial Port 1 */
#define VT1211_LDN_UART2	0x03	/* Serial Port 2 */
#define VT1211_LDN_MIDI		0x06	/* MIDI */
#define VT1211_LDN_GMP		0x07	/* Game Port */
#define VT1211_LDN_GPIO		0x08	/* GPIO */
#define VT1211_LDN_WDG		0x09	/* Watch Dog */
#define VT1211_LDN_WUC		0x0a	/* Wake-up Control */
#define VT1211_LDN_HM		0x0b	/* Hardware Monitor */
#define VT1211_LDN_VFIR		0x0c	/* Very Fast IR */
#define VT1211_LDN_ROM		0x0d	/* Flash ROM */

/* Watchdog Timer Control Registers (LDN 9) */
#define VT1211_WDG_ACT		0x30	/* Activate */
#define VT1211_WDG_ACT_EN		(1 << 0)	/* enabled */
#define VT1211_WDG_ADDR_MSB	0x60	/* Address [15:8] */
#define VT1211_WDG_ADDR_LSB	0x61	/* Address [7:0] */
#define VT1211_WDG_IRQSEL	0x70	/* IRQ Select */
#define VT1211_WDG_CONF		0xf0	/* Configuration */

/* Hardware Monitor Control Registers (LDN B) */
#define VT1211_HM_ACT		0x30	/* Activate */
#define VT1211_HM_ACT_EN		(1 << 0)	/* enabled */
#define VT1211_HM_ADDR_MSB	0x60	/* Address [15:8] */
#define VT1211_HM_ADDR_LSB	0x61	/* Address [7:0] */
#define VT1211_HM_IRQSEL	0x70	/* IRQ Select */

/* Watchdog Timer I/O Space Registers */
#define VT1211_WDG_STAT		0x00	/* Status */
#define VT1211_WDG_STAT_ACT		(1 << 0)	/* timer is active */
#define VT1211_WDG_MASK		0x01	/* Mask */
#define VT1211_WDG_MASK_COM1		(1 << 1)	/* COM1 trigger */
#define VT1211_WDG_MASK_COM2		(1 << 2)	/* COM2 trigger */
#define VT1211_WDG_TIMEOUT	0x02	/* Timeout */

#define VT1211_WDG_IOSIZE	0x04	/* Watchdog timer I/O space size */

/* Hardware Monitor I/O Space Registers */
#define VT1211_HM_SELD0		0x10	/* SELD[7:0] */
#define VT1211_HM_SELD1		0x11	/* SELD[15:8] */
#define VT1211_HM_SELD2		0x12	/* SELD[19:16] */
#define VT1211_HM_ADATA_MSB	0x13	/* Analog Data D[15:8] */
#define VT1211_HM_ADATA_LSB	0x14	/* Analog Data D[7:0] */
#define VT1211_HM_DDATA		0x15	/* Digital Data D[7:0] */
#define VT1211_HM_CHCNT		0x16	/* Channel Counter */
#define VT1211_HM_DVCI		0x17	/* Data Valid & Channel Indications */
#define VT1211_HM_SMBUSCTL	0x18	/* SMBus Control */
#define VT1211_HM_AFECTL	0x19	/* AFE Control */
#define VT1211_HM_AFETCTL	0x1a	/* AFE Test Control */
#define VT1211_HM_CHSET		0x1b	/* Channel Setting */
#define VT1211_HM_HTL3		0x1d	/* Hot Temp Limit 3 */
#define VT1211_HM_HTHL3		0x1e	/* Hot Temp Hysteresis Limit 3 */
#define VT1211_HM_TEMP1		0x1f	/* Temperature Reading 1 */
#define VT1211_HM_TEMP3		0x20	/* Temperature Reading 3 */
#define VT1211_HM_UCH1		0x21	/* UCH1 */
#define VT1211_HM_UCH2		0x22	/* UCH2 */
#define VT1211_HM_UCH3		0x23	/* UCH3 */
#define VT1211_HM_UCH4		0x24	/* UCH4 */
#define VT1211_HM_UCH5		0x25	/* UCH5 */
#define VT1211_HM_33V		0x26	/* +3.3V (Internal Vcc ) */
#define VT1211_HM_FAN1		0x29	/* FAN1 Reading */
#define VT1211_HM_FAN2		0x2a	/* FAN2 Reading */
#define VT1211_HM_UCH2HL	0x2b	/* UCH2 High Limit */
#define VT1211_HM_UCH2LL	0x2c	/* UCH2 Low Limit */
#define VT1211_HM_UCH3HL	0x2d	/* UCH3 High Limit */
#define VT1211_HM_UCH3LL	0x2e	/* UCH3 Low Limit */
#define VT1211_HM_UCH4HL	0x2f	/* UCH4 High Limit */
#define VT1211_HM_UCH4LL	0x30	/* UCH4 Low Limit */
#define VT1211_HM_UCH5HL	0x31	/* UCH5 High Limit */
#define VT1211_HM_UCH5LL	0x32	/* UCH5 Low Limit */
#define VT1211_HM_33VHL		0x33	/* Internal +3.3V High Limit */
#define VT1211_HM_33VLL		0x34	/* Internal +3.3V Low Limit */
#define VT1211_HM_HTL1		0x39	/* Hot Temp Limit 1 */
#define VT1211_HM_HTHL1		0x3a	/* Hot Temp Hysteresis Limit 1 */
#define VT1211_HM_FAN1CL	0x3b	/* FAN1 Fan Count Limit */
#define VT1211_HM_FAN2CL	0x3c	/* FAN2 Fan Count Limit */
#define VT1211_HM_UCH1HL	0x3d	/* UCH1 High Limit */
#define VT1211_HM_UCH1LL	0x3e	/* UCH1 Low Limit */
#define VT1211_HM_STEPID	0x3f	/* Stepping ID Number */
#define VT1211_HM_CONF		0x40	/* Configuration */
#define VT1211_HM_CONF_START		(1 << 0)
#define VT1211_HM_INTST1	0x41	/* Interrupt INT Status 1 */
#define VT1211_HM_INTST2	0x42	/* Interrupt INT Status 2 */
#define VT1211_HM_INTMASK1	0x43	/* INT Mask 1 */
#define VT1211_HM_INTMASK2	0x44	/* INT Mask 2 */
#define VT1211_HM_VID		0x45	/* VID */
#define VT1211_HM_OVOFCTL	0x46	/* Over Voltage & Over Fan Control */
#define VT1211_HM_FSCTL		0x47	/* Fan Speed Control */
#define VT1211_HM_FSCTL_DIV1(v)		(((v) >> 4) & 0x03)
#define VT1211_HM_FSCTL_DIV2(v)		(((v) >> 6) & 0x03)
#define VT1211_HM_SBA		0x48	/* Serial Bus Address */
#define VT1211_HM_VID4		0x49	/* VID 4 */
#define VT1211_HM_VID4_UCH1(v)		(((v) >> 4) & 0x03)
#define VT1211_HM_UCHCONF	0x4a	/* Universal Channel Configuration */
#define VT1211_HM_UCHCONF_ISTEMP(v, n)	(((v) & (1 << ((n) + 1))) != 0)
#define VT1211_HM_TCONF1	0x4b	/* Temperature Configuration 1 */
#define VT1211_HM_TCONF1_TEMP1(v)	(((v) >> 6) & 0x03)
#define VT1211_HM_TCONF2	0x4c	/* Temperature Configuration 2 */
#define VT1211_HM_ETR		0x4d	/* Extended Temperature Resolution */
#define VT1211_HM_ETR_UCH(v, n)		(((v) >> (((n) - 2) * 2)) & 0x03)
#define VT1211_HM_OTCTL		0x4e	/* Over Temperature Control */
#define VT1211_HM_PWMCS		0x50	/* PWM Clock Select */
#define VT1211_HM_PWMCTL	0x51	/* PWM Control */
#define VT1211_HM_PWMFST	0x52	/* PWM Full Speed Temperature Value */
#define VT1211_HM_PWMHST	0x53	/* PWM High Speed Temperature Value */
#define VT1211_HM_PWMLST	0x54	/* PWM Low Speed Temperature Value */
#define VT1211_HM_PWMFOT	0x55	/* PWM Fan Off Temperature Value */
#define VT1211_HM_PWMO1HSDC	0x56	/* PWM Output 1 Hi Speed Duty Cycle */
#define VT1211_HM_PWMO1LSDC	0x57	/* PWM Output 1 Lo Speed Duty Cycle */
#define VT1211_HM_PWMO2HSDC	0x58	/* PWM Output 2 Hi Speed Duty Cycle */
#define VT1211_HM_PWMO2LSDC	0x59	/* PWM Output 2 Lo Speed Duty Cycle */
#define VT1211_HM_PWMO3HSDC	0x5a	/* PWM Output 3 Hi Speed Duty Cycle */
#define VT1211_HM_PWMO3LSDC	0x5b	/* PWM Output 3 Lo Speed Duty Cycle */
#define VT1211_HM_BEEPEN	0x5c	/* BEEP Event Enable */
#define VT1211_HM_FEBFD		0x5d	/* Fan Event BEEP Frequency Divisor */
#define VT1211_HM_VEBFD		0x5e	/* Volt Event BEEP Frequency Divisor */
#define VT1211_HM_TEBFD		0x5f	/* Temp Event BEEP Frequency Divisor */
#define VT1211_HM_PWM1CDC	0x60	/* PWM1 Current Duty Cycle */
#define VT1211_HM_PWM2CDC	0x61	/* PWM2 Current Duty Cycle */

#define VT1211_HM_IOSIZE	0x80	/* Hardware monitor I/O space size */

/* PWM clock frequencies */
static const int vt1211_hm_clock[] = {
	90000, 45000, 22500, 11250, 5630, 2800, 1400, 700
};

/* Voltage inputs resistor factors */
static const int vt1211_hm_vrfact[] = {
	5952, 8333, 5952, 4167, 1754, 6296
};

/* Temperature conversion table */
static const struct {
	int raw;
	int64_t temp;
} vt1211_hm_temptbl[] = {
	{ 592, 298150000LLU },
	{ 600, 299130000LLU },
	{ 608, 300130000LLU },
	{ 616, 301140000LLU },
	{ 624, 302150000LLU },
	{ 632, 303170000LLU },
	{ 640, 304210000LLU },
	{ 648, 305250000LLU },
	{ 656, 306310000LLU },
	{ 664, 307380000LLU },
	{ 672, 308470000LLU },
	{ 680, 309570000LLU },
	{ 688, 310690000LLU },
	{ 696, 311830000LLU },
	{ 704, 312990000LLU },
	{ 712, 314170000LLU },
	{ 720, 315380000LLU },
	{ 728, 316610000LLU },
	{ 736, 317860000LLU },
	{ 744, 319150000LLU },
	{ 752, 320460000LLU },
	{ 760, 321810000LLU },
	{ 768, 323200000LLU },
	{ 776, 324620000LLU },
	{ 784, 326090000LLU },
	{ 792, 327610000LLU },
	{ 800, 329170000LLU },
	{ 808, 330790000LLU },
	{ 816, 332470000LLU },
	{ 824, 334220000LLU },
	{ 832, 336040000LLU },
	{ 840, 337940000LLU },
	{ 848, 339940000LLU },
	{ 856, 342030000LLU },
	{ 864, 344230000LLU },
	{ 872, 346560000LLU },
	{ 880, 349030000LLU },
	{ 888, 351670000LLU },
	{ 896, 354490000LLU },
	{ 904, 357530000LLU },
	{ 912, 360830000LLU },
	{ 920, 364430000LLU },
	{ 928, 368410000LLU },
	{ 936, 372830000LLU },
	{ 944, 377820000LLU },
	{ 952, 383530000LLU },
	{ 960, 390210000LLU },
	{ 968, 398230000LLU }	
};

/* Hardware monitor sensors */
enum {
	VT1211_HMS_TEMP1 = 0,
	VT1211_HMS_UCH1,
	VT1211_HMS_UCH2,
	VT1211_HMS_UCH3,
	VT1211_HMS_UCH4,
	VT1211_HMS_UCH5,
	VT1211_HMS_33V,
	VT1211_HMS_FAN1,
	VT1211_HMS_FAN2,
	VT1211_HM_NSENSORS		/* must be the last */
};

#endif	/* !_DEV_ISA_VIASIOREG_H_ */
