/* $OpenBSD: prcm.c,v 1.1 2009/05/08 03:13:26 drahn Exp $ */
/*
 * Copyright (c) 2007,2009 Dale Rahn <drahn@openbsd.org>
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/time.h>
#include <sys/device.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <arm/cpufunc.h>
#include <beagle/beagle/ahb.h>
#include <beagle/dev/prcmvar.h>

#define PRCM_REVISION		0x000
#define PRCM_SYSCONFIG		0x010
#define PRCM_IRQSTATUS_MPU	0x018
#define PRCM_IRQENABLE_MPU	0x01c
#define PRCM_VOLTCTRL		0x050
#define PRCM_VOLTST		0x054
#define PRCM_CLKSRC_CTRL	0x060
#define PRCM_CLKOUT_CTRL	0x070
#define PRCM_CLKEMUL_CTRL	0x078
#define PRCM_CLKCFG_CTRL	0x080
#define PRCM_CLKCFG_STATUS	0x084
#define PRCM_VOLTSETUP		0x090
#define PRCM_CLKSSETUP		0x094
#define PRCM_POLCTRL		0x098
#define PRCM_GP1		0x0b0
#define PRCM_GP2		0x0b4
#define PRCM_GP3		0x0b8
#define PRCM_GP4		0x0bc
#define PRCM_GP5		0x0c0
#define PRCM_GP6		0x0c4
#define PRCM_GP7		0x0c8
#define PRCM_GP8		0x0cc
#define PRCM_GP9		0x0d0
#define PRCM_GP10		0x0d4
#define PRCM_GP11		0x0d8
#define PRCM_GP12		0x0dc
#define PRCM_GP13		0x0e0
#define PRCM_GP14		0x0e4
#define PRCM_GP15		0x0e8
#define PRCM_GP16		0x0ec
#define PRCM_GP17		0x0f0
#define PRCM_GP18		0x0f4
#define PRCM_GP19		0x0f8
#define PRCM_GP20		0x0fc

#define CM_CLKSEL_MPU		0x140
#define  CM_CLKSTCTRL_MPU	0x148
#define  RM_RSTST_MPU		0x158
#define  PM_WKDEP_MPU		0x1C8
#define  PM_EVGENCTRL_MPU	0x1D4
#define  PM_EVEGENONTIM_MPU	0x1D8
#define  PM_EVEGENOFFTIM_MPU	0x1DC
#define  PM_PWSTCTRL_MPU	0x1E0
#define  PM_PWSTST_MPU		0x1E4
#define  CM_FCLKEN1_CORE	0x200
#define  	CM_FCLKEN1_CORE_DSS1	0x00000001
#define  	CM_FCLKEN1_CORE_DSS2	0x00000002
#define  	CM_FCLKEN1_CORE_TV	0x00000004
#define  	CM_FCLKEN1_CORE_VLYNQ	0x00000008
#define  	CM_FCLKEN1_CORE_GP2	0x00000010
#define  	CM_FCLKEN1_CORE_GP3	0x00000020
#define  	CM_FCLKEN1_CORE_GP4	0x00000040
#define  	CM_FCLKEN1_CORE_GP5	0x00000080
#define  	CM_FCLKEN1_CORE_GP6	0x00000100
#define  	CM_FCLKEN1_CORE_GP7	0x00000200
#define  	CM_FCLKEN1_CORE_GP8	0x00000400
#define  	CM_FCLKEN1_CORE_GP9	0x00000800
#define  	CM_FCLKEN1_CORE_GP10	0x00001000
#define  	CM_FCLKEN1_CORE_GP11	0x00002000
#define  	CM_FCLKEN1_CORE_GP12	0x00004000
#define  	CM_FCLKEN1_CORE_MCBSP1	0x00008000
#define  	CM_FCLKEN1_CORE_MCBSP2	0x00010000
#define  	CM_FCLKEN1_CORE_MCSPI1	0x00020000
#define  	CM_FCLKEN1_CORE_MCSPI2	0x00040000
#define  	CM_FCLKEN1_CORE_I2C1	0x00080000
#define  	CM_FCLKEN1_CORE_I2C2	0x00100000
#define  	CM_FCLKEN1_CORE_UART1	0x00200000
#define  	CM_FCLKEN1_CORE_UART2	0x00400000
#define  	CM_FCLKEN1_CORE_HDQ	0x00800000
#define  	CM_FCLKEN1_CORE_EAC	0x01000000
#define  	CM_FCLKEN1_CORE_FAC	0x02000000
#define  	CM_FCLKEN1_CORE_MMC	0x04000000
#define  	CM_FCLKEN1_CORE_MSPR0	0x08000000
#define  	CM_FCLKEN1_CORE_WDT3	0x10000000
#define  	CM_FCLKEN1_CORE_WDT4	0x20000000
#define  	CM_FCLKEN1_CORE_CAM	0x80000000
#define  CM_FCLKEN2_CORE	0x204
#define  	CM_FCLKEN2_CORE_UART3	0x00000004
#define  	CM_FCLKEN2_CORE_SSI	0x00000002
#define  	CM_FCLKEN2_CORE_USB	0x00000001
#define  CM_ICLKEN1_CORE	0x210
#define  	CM_ICLKEN1_CORE_DSS1	0x00000001
#define  	CM_ICLKEN1_CORE_VLYNQ	0x00000008
#define  	CM_ICLKEN1_CORE_GP2	0x00000010
#define  	CM_ICLKEN1_CORE_GP3	0x00000020
#define  	CM_ICLKEN1_CORE_GP4	0x00000040
#define  	CM_ICLKEN1_CORE_GP5	0x00000080
#define  	CM_ICLKEN1_CORE_GP6	0x00000100
#define  	CM_ICLKEN1_CORE_GP7	0x00000200
#define  	CM_ICLKEN1_CORE_GP8	0x00000400
#define  	CM_ICLKEN1_CORE_GP9	0x00000800
#define  	CM_ICLKEN1_CORE_GP10	0x00001000
#define  	CM_ICLKEN1_CORE_GP11	0x00002000
#define  	CM_ICLKEN1_CORE_GP12	0x00004000
#define  	CM_ICLKEN1_CORE_MCBSP1	0x00008000
#define  	CM_ICLKEN1_CORE_MCBSP2	0x00010000
#define  	CM_ICLKEN1_CORE_MCSPI1	0x00020000
#define  	CM_ICLKEN1_CORE_MCSPI2	0x00040000
#define  	CM_ICLKEN1_CORE_I2C1	0x00080000
#define  	CM_ICLKEN1_CORE_I2C2	0x00100000
#define  	CM_ICLKEN1_CORE_UART1	0x00200000
#define  	CM_ICLKEN1_CORE_UART2	0x00400000
#define  	CM_ICLKEN1_CORE_HDQ	0x00800000
#define  	CM_ICLKEN1_CORE_EAC	0x01000000
#define  	CM_ICLKEN1_CORE_FAC	0x02000000
#define  	CM_ICLKEN1_CORE_MMC	0x04000000
#define  	CM_ICLKEN1_CORE_MSPR0	0x08000000
#define  	CM_ICLKEN1_CORE_WDT3	0x10000000
#define  	CM_ICLKEN1_CORE_WDT4	0x20000000
#define  	CM_ICLKEN1_CORE_MAILBOX	0x40000000
#define  	CM_ICLKEN1_CORE_CAM	0x80000000
#define  CM_ICLKEN2_CORE	0x214
#define  CM_ICLKEN4_CORE	0x21C
#define  CM_IDLEST1_CORE	0x220
#define  CM_IDLEST2_CORE	0x224
#define  CM_IDLEST4_CORE	0x22C
#define  CM_AUTOIDLE1_CORE	0x230
#define  CM_AUTOIDLE2_CORE	0x234
#define  CM_AUTOIDLE3_CORE	0x238
#define  CM_AUTOIDLE4_CORE	0x23C
#define  CM_CLKSEL1_CORE	0x240
#define  CM_CLKSEL2_CORE	0x244
#define  CM_CLKSTCTRL_CORE	0x248
#define  PM_WKEN1_CORE		0x2A0
#define  PM_WKEN2_CORE		0x2A4
#define  PM_WKST1_CORE		0x2B0
#define  PM_WKST2_CORE		0x2B4
#define  PM_WKDEP_CORE		0x2C8
#define  PM_PWSTCTRL_CORE	0x2E0
#define  PM_PWSTST_CORE		0x2E4
#define  CM_FCLKEN_GFX		0x300
#define  CM_ICLKEN_GFX		0x310

#define CM_IDLEST_GFX		0x320
#define CM_CLKSEL_GFX		0x340
#define CM_CLKSTCTRL_GFX	0x348
#define RM_RSTCTRL_GFX		0x350
#define RM_RSTST_GFX		0x358
#define PM_WKDEP_GFX		0x3C8
#define PM_PWSTCTRL_GFX		0x3E0
#define PM_PWSTST_GFX		0x3E4
#define CM_FCLKEN_WKUP		0x400
#define		CM_FCLKEN_WKUP_GPT1	1
#define		CM_FCLKEN_WKUP_GPIOS	4
#define		CM_FCLKEN_WKUP_MPU_WDT	8
#define CM_ICLKEN_WKUP		0x410
#define		CM_ICLKEN_WKUP_GPT1	0x01
#define		CM_ICLKEN_WKUP_32KSYNC	0x02
#define		CM_ICLKEN_WKUP_GPIOS	0x04
#define		CM_ICLKEN_WKUP_MPU_WDT	0x08
#define		CM_ICLKEN_WKUP_WDT1	0x10
#define		CM_ICLKEN_WKUP_OMAPCTRL	0x20
#define	CM_IDLEST_WKUP		0x420
#define CM_AUTOIDLE_WKUP	0x430
#define CM_CLKSEL_WKUP		0x440
#define RM_RSTCTRL_WKUP		0x450
#define RM_RSTTIME_WKUP		0x454
#define RM_RSTST_WKUP		0x458
#define PM_WKEN_WKUP		0x4A0
#define PM_WKST_WKUP		0x4B0
#define CM_CLKEN_PLL		0x500
#define CM_IDLEST_CKGEN		0x520
#define CM_AUTOIDLE_PLL		0x530
#define CM_CLKSEL1_PLL		0x540
#define CM_CLKSEL2_PLL		0x544
#define CM_FCLKEN_DSP		0x800
#define CM_ICLKEN_DSP		0x810
#define CM_IDLEST_DSP		0x820
#define CM_AUTOIDLE_DSP		0x830
#define CM_CLKSEL_DSP		0x840
#define CM_CLKSTCTRL_DSP	0x848
#define RM_RSTCTRL_DSP		0x850
#define RM_RSTST_DSP		0x858
#define PM_WKEN_DSP		0x8A0
#define PM_WKDEP_DSP		0x8C8
#define PM_PWSTCTRL_DSP		0x8E0
#define PM_PWSTST_DSP		0x8E4
#define PRCM_IRQSTATUS_DSP	0x8F0
#define PRCM_IRQENABLE_DSP	0x8F4
#define PRCM_IRQSTATUS_IVA	0x8F8
#define PRCM_IRQENABLE_IVA	0x8FC
#define PRCM_SIZE	0x1000

bus_space_tag_t prcm_iot;
bus_space_handle_t prcm_ioh;
int prcm_attached;

int     prcm_match(struct device *, void *, void *);
void    prcm_attach(struct device *, struct device *, void *);



struct cfattach	prcm_ca = {
	sizeof (struct device), prcm_match, prcm_attach
};

struct cfdriver prcm_cd = {
	NULL, "prcm", DV_DULL
};

int
prcm_match(struct device *parent, void *v, void *aux)
{
	/* only attach once */
	if (prcm_attached != 0)
		return (0);
	return (1);
}

void
prcm_attach(struct device *parent, struct device *self, void *args)
{
        struct ahb_attach_args *aa = args;
	prcm_iot = aa->aa_iot;
	u_int32_t reg;

	if (bus_space_map(prcm_iot, aa->aa_addr, PRCM_SIZE, 0, &prcm_ioh))
		panic("prcm_attach: bus_space_map failed!");

	reg = bus_space_read_4(prcm_iot, prcm_ioh, PRCM_REVISION);
	printf(" rev %d.%d\n", reg >> 4 & 0xf, reg & 0xf);
	
	prcm_attached = 1;

	/* XXX */
#if 1
	printf("CM_FCLKEN1_CORE %x\n", bus_space_read_4(prcm_iot, prcm_ioh, CM_FCLKEN1_CORE));
	printf("CM_ICLKEN1_CORE %x\n", bus_space_read_4(prcm_iot, prcm_ioh, CM_ICLKEN1_CORE));
	printf("CM_AUTOIDLE1_CORE %x\n", bus_space_read_4(prcm_iot, prcm_ioh, CM_AUTOIDLE1_CORE));

	printf("CM_FCLKEN_WKUP %x\n", bus_space_read_4(prcm_iot, prcm_ioh, CM_FCLKEN_WKUP));
	printf(" CM_IDLEST_WKUP %x\n", bus_space_read_4(prcm_iot, prcm_ioh,  CM_IDLEST_WKUP));
//	bus_space_write_4(prcm_iot, prcm_ioh, 
#endif

	reg = bus_space_read_4(prcm_iot, prcm_ioh, CM_FCLKEN1_CORE);
	reg |= CM_FCLKEN1_CORE_GP3|CM_FCLKEN1_CORE_GP2;
	bus_space_write_4(prcm_iot, prcm_ioh, CM_FCLKEN1_CORE, reg);
	reg = bus_space_read_4(prcm_iot, prcm_ioh, CM_ICLKEN1_CORE);
	reg |= CM_ICLKEN1_CORE_GP3|CM_ICLKEN1_CORE_GP2;
	bus_space_write_4(prcm_iot, prcm_ioh, CM_ICLKEN1_CORE, reg);

	reg = bus_space_read_4(prcm_iot, prcm_ioh, CM_FCLKEN_WKUP);
	reg |= CM_FCLKEN_WKUP_MPU_WDT | CM_FCLKEN_WKUP_GPT1;
	bus_space_write_4(prcm_iot, prcm_ioh, CM_FCLKEN_WKUP, reg);

	reg = bus_space_read_4(prcm_iot, prcm_ioh, CM_ICLKEN_WKUP);
	reg |= CM_ICLKEN_WKUP_MPU_WDT | CM_ICLKEN_WKUP_GPT1;
	bus_space_write_4(prcm_iot, prcm_ioh, CM_ICLKEN_WKUP, reg);
}

void
prcm_setclock(int clock, int speed)
{
#if 0
	u_int32_t reg;
	if (clock == 0) {
		reg = bus_space_read_4(prcm_iot, prcm_ioh, CM_CLKSEL_WKUP);
		reg &= ~( 3 << ((clock -1) *2 ));
		reg |=  ( speed << ((clock -1) *2 ));
		bus_space_write_4(prcm_iot, prcm_ioh, CM_CLKSEL_WKUP, reg);
	} else if (clock > 0 && clock < 13) {
		reg = bus_space_read_4(prcm_iot, prcm_ioh, CM_CLKSEL2_CORE);

		reg &= ~( 3 << (clock * 2));
		reg |=  ( speed << ((clock -1) *2 ));

		bus_space_write_4(prcm_iot, prcm_ioh, CM_CLKSEL2_CORE, reg);
	} else
		panic("prcm_setclock invalid clock %d\n", clock);
#endif
}

void
prcm_enableclock(int bit)
{
#if 0
	u_int32_t fclk, iclk;
	int freg, ireg;

	if (bit < 31){
		freg = CM_FCLKEN1_CORE;
		ireg = CM_ICLKEN1_CORE;
	} else {
		freg = CM_FCLKEN2_CORE;
		ireg = CM_ICLKEN2_CORE;
	}

	fclk = bus_space_read_4(prcm_iot, prcm_ioh, freg);
	iclk = bus_space_read_4(prcm_iot, prcm_ioh, ireg);
	fclk |=  1 << (bit & 0x1f);
	iclk |=  1 << (bit & 0x1f);

	/* mask reserved bits (XXX?) */
	if (bit > 31){
		fclk &= 0xbfffffff;
		iclk &= 0xfffffff9;
	} else {
		fclk &= 0x00000007;
		iclk &= 0x00000007;
	}
	bus_space_write_4(prcm_iot, prcm_ioh, freg, fclk);
	bus_space_write_4(prcm_iot, prcm_ioh, ireg, iclk);
#endif
}
