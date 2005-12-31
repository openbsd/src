/*	$OpenBSD: wm8750reg.h,v 1.2 2005/12/31 04:31:27 deraadt Exp $	*/

/*
 * Copyright (c) 2005 Christopher Pascoe <c.pascoe@itee.uq.edu.au>
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

/*
 * Wolfson Microelectronics' WM8750 I2C/I2S audio codec:
 * - I2C register definitions.  Used in the Sharp Zaurus SL-C3000.
 */

#define LINVOL_REG 0x00			/* Left Input volume */
#define  LINVOL_LIVU 0x100
#define  LINVOL_LINMUTE 0x80
#define  LINVOL_LIZC 0x40
#define  LINVOL_LINVOL_MASK 0x3F
#define  LINVOL_GET_LINVOL(x) ((x) & 0x3F)
#define  LINVOL_SET_LINVOL(x) (x)

#define RINVOL_REG 0x01			/* Right Input volume */
#define  RINVOL_RIVU 0x100
#define  RINVOL_RINMUTE 0x80
#define  RINVOL_RIZC 0x40
#define  RINVOL_RINVOL_MASK 0x3F
#define  RINVOL_GET_RINVOL(x) ((x) & 0x3F)
#define  RINVOL_SET_RINVOL(x) (x)

#define LOUT1VOL_REG 0x02		/* LOUT1 volume */
#define  LOUT1VOL_LO1VU 0x100
#define  LOUT1VOL_LO1ZC 0x80
#define  LOUT1VOL_LOUT1VOL_MASK 0x7F
#define  LOUT1VOL_GET_LOUT1VOL(x) ((x) & 0x7F)
#define  LOUT1VOL_SET_LOUT1VOL(x) (x)

#define ROUT1VOL_REG 0x03		/* ROUT1 volume */
#define  ROUT1VOL_RO1VU 0x100
#define  ROUT1VOL_RO1ZC 0x80
#define  ROUT1VOL_ROUT1VOL_MASK 0x7F
#define  ROUT1VOL_GET_ROUT1VOL(x) ((x) & 0x7F)
#define  ROUT1VOL_SET_ROUT1VOL(x) (x)

#define ADCDACCTL_REG 0x05		/* ADC & DAC Control */
#define  ADCDACCTL_ADCDIV2 0x100
#define  ADCDACCTL_DACDIV2 0x80
#define  ADCDACCTL_ADCPOL_MASK 0x60
#define  ADCDACCTL_GET_ADCPOL(x) (((x) >> 5) & 0x60)
#define  ADCDACCTL_SET_ADCPOL(x) ((x) << 5)
#define  ADCDACCTL_HPOR 0x10
#define  ADCDACCTL_DACMU 0x8
#define  ADCDACCTL_DEEMPH_MASK 0x6
#define  ADCDACCTL_GET_DEEMPH(x) (((x) >> 1) & 0x6)
#define  ADCDACCTL_SET_DEEMPH(x) ((x) << 1)
#define  ADCDACCTL_ADCHPD 0x1

#define AUDINT_REG 0x07			/* Audio Interface */
#define  AUDINT_BCLKINV 0x80
#define  AUDINT_MS 0x40
#define  AUDINT_LRSWAP 0x20
#define  AUDINT_LRP 0x10
#define  AUDINT_WL_MASK 0xC
#define  AUDINT_GET_WL(x) (((x) >> 2) & 0xC)
#define  AUDINT_SET_WL(x) ((x) << 2)
#define  AUDINT_FORMAT_MASK 0x3
#define  AUDINT_GET_FORMAT(x) ((x) & 0x3)
#define  AUDINT_SET_FORMAT(x) (x)

#define SRATE_REG 0x08			/* Sample rate */
#define  SRATE_BCM_MASK 0x180
#define  SRATE_GET_BCM(x) (((x) >> 7) & 0x180)
#define  SRATE_SET_BCM(x) ((x) << 7)
#define  SRATE_CLKDIV2 0x40
#define  SRATE_SR_MASK 0x3E
#define  SRATE_GET_SR(x) (((x) >> 1) & 0x3E)
#define  SRATE_SET_SR(x) ((x) << 1)
#define  SRATE_USB 0x1

#define LDACVOL_REG 0x0A		/* Left DAC volume */
#define  LDACVOL_LDVU 0x100
#define  LDACVOL_LDACVOL_MASK 0xFF
#define  LDACVOL_GET_LDACVOL(x) ((x) & 0xFF)
#define  LDACVOL_SET_LDACVOL(x) (x)

#define RDACVOL_REG 0x0B		/* Right DAC volume */
#define  RDACVOL_RDVU 0x100
#define  RDACVOL_RDACVOL_MASK 0xFF
#define  RDACVOL_GET_RDACVOL(x) ((x) & 0xFF)
#define  RDACVOL_SET_RDACVOL(x) (x)

#define BASSCTL_REG 0x0C		/* Bass control */
#define  BASSCTL_BB 0x80
#define  BASSCTL_BC 0x40
#define  BASSCTL_BASS_MASK 0xF
#define  BASSCTL_GET_BASS(x) ((x) & 0xF)
#define  BASSCTL_SET_BASS(x) (x)

#define TREBCTL_REG 0x0D		/* Treble control */
#define  TREBCTL_TC 0x40
#define  TREBCTL_TRBL_MASK 0xF
#define  TREBCTL_GET_TRBL(x) ((x) & 0xF)
#define  TREBCTL_SET_TRBL(x) (x)

#define RESET_REG 0x0F			/* Reset */

#define C3DCTL_REG 0x10			/* 3D control */
#define  C3DCTL_MODE3D 0x80
#define  C3DCTL_3DUC 0x40
#define  C3DCTL_3DLC 0x20
#define  C3DCTL_3DDEPTH_MASK 0x1E
#define  C3DCTL_GET_3DDEPTH(x) (((x) >> 1) & 0x1E)
#define  C3DCTL_SET_3DDEPTH(x) ((x) << 1)
#define  C3DCTL_3DEN 0x1

#define ALC1_REG 0x11			/* ALC1 */
#define  ALC1_ALCSEL_MASK 0x180
#define  ALC1_GET_ALCSEL(x) (((x) >> 7) & 0x180)
#define  ALC1_SET_ALCSEL(x) ((x) << 7)
#define  ALC1_MAXGAIN_MASK 0x70
#define  ALC1_GET_MAXGAIN(x) (((x) >> 4) & 0x70)
#define  ALC1_SET_MAXGAIN(x) ((x) << 4)
#define  ALC1_ALCL_MASK 0xF
#define  ALC1_GET_ALCL(x) ((x) & 0xF)
#define  ALC1_SET_ALCL(x) (x)

#define ALC2_REG 0x12			/* ALC2 */
#define  ALC2_ALCZC 0x80
#define  ALC2_HLD_MASK 0xF
#define  ALC2_GET_HLD(x) ((x) & 0xF)
#define  ALC2_SET_HLD(x) (x)

#define ALC3_REG 0x13			/* ALC3 */
#define  ALC3_DCY_MASK 0xF0
#define  ALC3_GET_DCY(x) (((x) >> 4) & 0xF0)
#define  ALC3_SET_DCY(x) ((x) << 4)
#define  ALC3_ATK_MASK 0xF
#define  ALC3_GET_ATK(x) ((x) & 0xF)
#define  ALC3_SET_ATK(x) (x)

#define NOISEGATE_REG 0x14		/* Noise Gate */
#define  NOISEGATE_NGTH_MASK 0xF8
#define  NOISEGATE_GET_NGTH(x) (((x) >> 3) & 0xF8)
#define  NOISEGATE_SET_NGTH(x) ((x) << 3)
#define  NOISEGATE_NGG_MASK 0x6
#define  NOISEGATE_GET_NGG(x) (((x) >> 1) & 0x6)
#define  NOISEGATE_SET_NGG(x) ((x) << 1)
#define  NOISEGATE_NGAT 0x1

#define LADCVOL_REG 0x15		/* Left ADC volume */
#define  LADCVOL_LAVU 0x100
#define  LADCVOL_LADCVOL_MASK 0xFF
#define  LADCVOL_GET_LADCVOL(x) ((x) & 0xFF)
#define  LADCVOL_SET_LADCVOL(x) (x)

#define RADCVOL_REG 0x16		/* Right ADC volume */
#define  RADCVOL_RAVU 0x100
#define  RADCVOL_RADCVOL_MASK 0xFF
#define  RADCVOL_GET_RADCVOL(x) ((x) & 0xFF)
#define  RADCVOL_SET_RADCVOL(x) (x)

#define ADCTL1_REG 0x17			/* Additional control(1) */
#define  ADCTL1_TSDEN 0x100
#define  ADCTL1_VSEL_MASK 0xC0
#define  ADCTL1_GET_VSEL(x) (((x) >> 6) & 0xC0)
#define  ADCTL1_SET_VSEL(x) ((x) << 6)
#define  ADCTL1_DMONOMIX_MASK 0x30
#define  ADCTL1_GET_DMONOMIX(x) (((x) >> 4) & 0x30)
#define  ADCTL1_SET_DMONOMIX(x) ((x) << 4)
#define  ADCTL1_DATSEL_MASK 0xC
#define  ADCTL1_GET_DATSEL(x) (((x) >> 2) & 0xC)
#define  ADCTL1_SET_DATSEL(x) ((x) << 2)
#define  ADCTL1_DACINV 0x2
#define  ADCTL1_TOEN 0x1

#define ADCTL2_REG 0x18			/* Additional control(2) */
#define  ADCTL2_OUTSW3_MASK 0x180
#define  ADCTL2_GET_OUTSW3(x) (((x) >> 7) & 0x180)
#define  ADCTL2_SET_OUTSW3(x) ((x) << 7)
#define  ADCTL2_HPSWEN 0x40
#define  ADCTL2_HPSWPOL 0x20
#define  ADCTL2_ROUT2INV 0x10
#define  ADCTL2_TRI 0x08
#define  ADCTL2_LRCM 0x04
#define  ADCTL2_ADCOSR 0x02
#define  ADCTL2_DACOSR 0x01

#define PWRMGMT1_REG 0x19		/* Pwr Mgmt (1) */
#define  PWRMGMT1_VMIDSEL_MASK 0x180
#define  PWRMGMT1_GET_VMIDSEL(x) (((x) >> 7) & 0x180)
#define  PWRMGMT1_SET_VMIDSEL(x) ((x) << 7)
#define  PWRMGMT1_VREF 0x40
#define  PWRMGMT1_AINL 0x20
#define  PWRMGMT1_AINR 0x10
#define  PWRMGMT1_ADCL 0x8
#define  PWRMGMT1_ADCR 0x4
#define  PWRMGMT1_MICB 0x2
#define  PWRMGMT1_DIGENB 0x1

#define PWRMGMT2_REG 0x1A		/* Pwr Mgmt (2) */
#define  PWRMGMT2_DACL 0x100
#define  PWRMGMT2_DACR 0x80
#define  PWRMGMT2_LOUT1 0x40
#define  PWRMGMT2_ROUT1 0x20
#define  PWRMGMT2_LOUT2 0x10
#define  PWRMGMT2_ROUT2 0x8
#define  PWRMGMT2_MONO 0x4
#define  PWRMGMT2_OUT3 0x2

#define ADCTL3_REG 0x1B			/* Additional Control (3) */
#define  ADCTL3_ADCLRM_MASK 0x180
#define  ADCTL3_GET_ADCLRM(x) (((x) >> 7) & 0x180)
#define  ADCTL3_SET_ADCLRM(x) ((x) << 7)
#define  ADCTL3_VROI 0x40
#define  ADCTL3_HPFLREN 0x20

#define ADCINPMODE_REG 0x1F		/* ADC input mode */
#define  ADCINPMODE_DS 0x100
#define  ADCINPMODE_MONOMIX_MASK 0xC0
#define  ADCINPMODE_GET_MONOMIX(x) (((x) >> 6) & 0xC0)
#define  ADCINPMODE_SET_MONOMIX(x) ((x) << 6)
#define  ADCINPMODE_RDCM 0x20
#define  ADCINPMODE_LDCM 0x10

#define ADCLSPATH_REG 0x20		/* ADCL signal path */
#define  ADCLSPATH_LINSEL_MASK 0xC0
#define  ADCLSPATH_GET_LINSEL(x) (((x) >> 6) & 0xC0)
#define  ADCLSPATH_SET_LINSEL(x) ((x) << 6)
#define  ADCLSPATH_LMICBOOST_MASK 0x30
#define  ADCLSPATH_GET_LMICBOOST(x) (((x) >> 4) & 0x30)
#define  ADCLSPATH_SET_LMICBOOST(x) ((x) << 4)

#define ADCRSPATH_REG 0x21		/* ADCR signal path */
#define  ADCRSPATH_RINSEL_MASK 0xC0
#define  ADCRSPATH_GET_RINSEL(x) (((x) >> 6) & 0xC0)
#define  ADCRSPATH_SET_RINSEL(x) ((x) << 6)
#define  ADCRSPATH_RMICBOOST_MASK 0x30
#define  ADCRSPATH_GET_RMICBOOST(x) (((x) >> 4) & 0x30)
#define  ADCRSPATH_SET_RMICBOOST(x) ((x) << 4)

#define LOUTMIX1_REG 0x22		/* Left out Mix (1) */
#define  LOUTMIX1_LD2LO 0x100
#define  LOUTMIX1_LI2LO 0x80
#define  LOUTMIX1_LI2LOVOL_MASK 0x70
#define  LOUTMIX1_GET_LI2LOVOL(x) (((x) >> 4) & 0x70)
#define  LOUTMIX1_SET_LI2LOVOL(x) ((x) << 4)
#define  LOUTMIX1_LMIXSEL_MASK 0x7
#define  LOUTMIX1_GET_LMIXSEL(x) ((x) & 0x7)
#define  LOUTMIX1_SET_LMIXSEL(x) (x)

#define LOUTMIX2_REG 0x23		/* Left out Mix (2) */
#define  LOUTMIX2_RD2LO 0x100
#define  LOUTMIX2_RI2LO 0x80
#define  LOUTMIX2_RI2LOVOL_MASK 0x70
#define  LOUTMIX2_GET_RI2LOVOL(x) (((x) >> 4) & 0x70)
#define  LOUTMIX2_SET_RI2LOVOL(x) ((x) << 4)

#define ROUTMIX1_REG 0x24		/* Right out Mix (1) */
#define  ROUTMIX1_LD2RO 0x100
#define  ROUTMIX1_LI2RO 0x80
#define  ROUTMIX1_LI2ROVOL_MASK 0x70
#define  ROUTMIX1_GET_LI2ROVOL(x) (((x) >> 4) & 0x70)
#define  ROUTMIX1_SET_LI2ROVOL(x) ((x) << 4)
#define  ROUTMIX1_RMIXSEL_MASK 0x7
#define  ROUTMIX1_GET_RMIXSEL(x) ((x) & 0x7)
#define  ROUTMIX1_SET_RMIXSEL(x) (x)

#define ROUTMIX2_REG 0x25		/* Right out Mix (2) */
#define  ROUTMIX2_RD2RO 0x100
#define  ROUTMIX2_RI2RO 0x80
#define  ROUTMIX2_RI2ROVOL_MASK 0x70
#define  ROUTMIX2_GET_RI2ROVOL(x) (((x) >> 4) & 0x70)
#define  ROUTMIX2_SET_RI2ROVOL(x) ((x) << 4)

#define MOUTMIX1_REG 0x26		/* Mono out Mix (1) */
#define  MOUTMIX1_LD2MO 0x100
#define  MOUTMIX1_LI2MO 0x80
#define  MOUTMIX1_LI2MOVOL_MASK 0x70
#define  MOUTMIX1_GET_LI2MOVOL(x) (((x) >> 4) & 0x70)
#define  MOUTMIX1_SET_LI2MOVOL(x) ((x) << 4)

#define MOUTMIX2_REG 0x27		/* Mono out Mix (2) */
#define  MOUTMIX2_RD2MO 0x100
#define  MOUTMIX2_RI2MO 0x80
#define  MOUTMIX2_RI2MOVOL_MASK 0x70
#define  MOUTMIX2_GET_RI2MOVOL(x) (((x) >> 4) & 0x70)
#define  MOUTMIX2_SET_RI2MOVOL(x) ((x) << 4)

#define LOUT2VOL_REG 0x28		/* LOUT2 volume */
#define  LOUT2VOL_LO2VU 0x100
#define  LOUT2VOL_LO2ZC 0x80
#define  LOUT2VOL_LOUT2VOL_MASK 0x7F
#define  LOUT2VOL_GET_LOUT2VOL(x) ((x) & 0x7F)
#define  LOUT2VOL_SET_LOUT2VOL(x) (x)

#define ROUT2VOL_REG 0x29		/* ROUT2 volume */
#define  ROUT2VOL_RO2VU 0x100
#define  ROUT2VOL_RO2ZC 0x80
#define  ROUT2VOL_ROUT2VOL_MASK 0x7F
#define  ROUT2VOL_GET_ROUT2VOL(x) ((x) & 0x7F)
#define  ROUT2VOL_SET_ROUT2VOL(x) (x)

#define MOUTVOL_REG 0x2A		/* MONOOUT volume */
#define  MOUTVOL_MOZC 0x80
#define  MOUTVOL_MOUTVOL_MASK 0x7F
#define  MOUTVOL_GET_MOUTVOL(x) ((x) & 0x7F)
#define  MOUTVOL_SET_MOUTVOL(x) (x)

