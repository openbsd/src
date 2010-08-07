/* $OpenBSD: prcm.c,v 1.4 2010/08/07 03:50:01 krw Exp $ */
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

#define CM_FCLKEN_IVA2		0x0000
#define CM_CLKEN_PLL_IVA2	0x0004
#define CM_IDLEST_IVA2		0x0020
#define CM_IDLEST_PLL_IVA2	0x0024
#define CM_AUTOIDLE_PLL_IVA2	0x0034
#define CM_CLKSEL1_PLL_IVA2	0x0040
#define CM_CLKSEL2_PLL_IVA2	0x0044
#define CM_CLKSTCTRL_IVA2	0x0048
#define CM_CLKSTST_IVA2		0x004c

#define PRCM_REVISION		0x0800
#define PRCM_SYSCONFIG		0x0810

#define	CM_CLKSEL_MPU		0x0940
#define CM_CLKSTCTRL_MPU	0x0948
#define RM_RSTST_MPU		0x0958
#define PM_WKDEP_MPU		0x09C8
#define PM_EVGENCTRL_MPU	0x09D4
#define PM_EVEGENONTIM_MPU	0x09D8
#define PM_EVEGENOFFTIM_MPU	0x09DC
#define PM_PWSTCTRL_MPU		0x09E0
#define PM_PWSTST_MPU		0x09E4

#define  CM_ICLKEN1_CORE	0x0a10
#define  CM_ICLKEN1_CORE_MSK	0x7ffffed2
#define  CM_ICLKEN2_CORE	0x0a14
#define  CM_ICLKEN2_CORE_MSK	0x0000001f
#define  CM_ICLKEN3_CORE	0x0a18
#define  CM_ICLKEN3_CORE_MSK	0x00000004
#define  CM_ICLKEN4_CORE	0x0a1C
#define  CM_IDLEST1_CORE	0x0a20
#define  CM_IDLEST2_CORE	0x0a24
#define  CM_IDLEST4_CORE	0x0a2C
#define  CM_AUTOIDLE1_CORE	0x0a30
#define  CM_AUTOIDLE2_CORE	0x0a34
#define  CM_AUTOIDLE3_CORE	0x0a38
#define  CM_AUTOIDLE4_CORE	0x0a3C
#define  CM_CLKSEL1_CORE	0x0a40
#define  CM_CLKSEL2_CORE	0x0a44
#define  CM_CLKSTCTRL_CORE	0x0a48
#define  PM_WKEN1_CORE		0x0aA0
#define  PM_WKEN2_CORE		0x0aA4
#define  PM_WKST1_CORE		0x0aB0
#define  PM_WKST2_CORE		0x0aB4
#define  PM_WKDEP_CORE		0x0aC8
#define  PM_PWSTCTRL_CORE	0x0aE0
#define  PM_PWSTST_CORE		0x0aE4
#define  CM_FCLKEN_GFX		0x0b00
#define  CM_ICLKEN_GFX		0x0b10

#define CM_IDLEST_GFX		0x0b20
#define CM_CLKSEL_GFX		0x0b40
#define CM_CLKSTCTRL_GFX	0x0b48
#define RM_RSTCTRL_GFX		0x0b50
#define RM_RSTST_GFX		0x0b58
#define PM_WKDEP_GFX		0x0bC8
#define PM_PWSTCTRL_GFX		0x0bE0
#define PM_PWSTST_GFX		0x0bE4
#define CM_FCLKEN_WKUP		0x0c00
#define		CM_FCLKEN_WKUP_GPT1	1
#define		CM_FCLKEN_WKUP_GPIOS	4
#define		CM_FCLKEN_WKUP_MPU_WDT	8
#define CM_ICLKEN_WKUP		0xc10
#define		CM_ICLKEN_WKUP_GPT1	0x01
#define		CM_ICLKEN_WKUP_32KSYNC	0x02
#define		CM_ICLKEN_WKUP_GPIOS	0x04
#define		CM_ICLKEN_WKUP_MPU_WDT	0x08
#define		CM_ICLKEN_WKUP_WDT1	0x10
#define		CM_ICLKEN_WKUP_OMAPCTRL	0x20
#define	CM_IDLEST_WKUP		0x0c20
#define CM_AUTOIDLE_WKUP	0x0c30
#define CM_CLKSEL_WKUP		0x0c40
#define RM_RSTCTRL_WKUP		0x0c50
#define RM_RSTTIME_WKUP		0x0c54
#define RM_RSTST_WKUP		0x0c58
#define PM_WKEN_WKUP		0x0cA0
#define PM_WKST_WKUP		0x0cB0
#define CM_CLKEN_PLL		0x0d00
#define CM_IDLEST_CKGEN		0x0d20
#define CM_AUTOIDLE_PLL		0x0d30
#define CM_CLKSEL1_PLL		0x0d40
#define CM_CLKSEL2_PLL		0x0d44
#define CM_FCLKEN_PER		0x1000
#define CM_ICLKEN_PER		0x1010
#define CM_IDLEST_PER		0x1020
#define CM_AUTOIDLE_PER		0x1030
#define CM_CLKSEL_PER		0x1040
#define CM_SLEEPDEP_PER		0x1044
#define CM_CLKSTCTRL_PER	0x1048
#define CM_CLKSTST_PER		0x104C

#define CM_CLKSEL1_EMU		0x5140
#define CM_CLKSTCTRL_EMU	0x5148
#define CM_CLKSTST_EMU		0x514C
#define CM_CLKSEL2_EMU		0x5150
#define CM_CLKSEL3_EMU		0x5154

#define CM_POLCTRL		0x529C

#define CM_IDLEST_NEON		0x5320
#define CM_CLKSTCTRL_NEON	0x5348

#define CM_FCLKEN_USBHOST	0x5400
#define CM_ICLKEN_USBHOST	0x5410
#define CM_IDLEST_USBHOST	0x5420
#define CM_AUTOIDLE_USBHOST	0x5430
#define CM_SLEEPDEP_USBHOST	0x5444
#define CM_CLKSTCTRL_USBHOST	0x5448
#define CM_CLKSTST_USBHOST	0x544C

uint32_t prcm_imask_cur[PRCM_REG_MAX];
uint32_t prcm_fmask_cur[PRCM_REG_MAX];
uint32_t prcm_imask_mask[PRCM_REG_MAX];
uint32_t prcm_fmask_mask[PRCM_REG_MAX];
uint32_t prcm_imask_addr[PRCM_REG_MAX];
uint32_t prcm_fmask_addr[PRCM_REG_MAX];

#define PRCM_SIZE	0x2000

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

#if 0
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
#endif
	prcm_fmask_mask[PRCM_REG_CORE_CLK1] = PRCM_REG_CORE_CLK1_FMASK;
	prcm_imask_mask[PRCM_REG_CORE_CLK1] = PRCM_REG_CORE_CLK1_IMASK;
	prcm_fmask_addr[PRCM_REG_CORE_CLK1] = PRCM_REG_CORE_CLK1_FADDR;
	prcm_imask_addr[PRCM_REG_CORE_CLK1] = PRCM_REG_CORE_CLK1_IADDR;

	prcm_fmask_mask[PRCM_REG_CORE_CLK2] = PRCM_REG_CORE_CLK2_FMASK;
	prcm_imask_mask[PRCM_REG_CORE_CLK2] = PRCM_REG_CORE_CLK2_IMASK;
	prcm_fmask_addr[PRCM_REG_CORE_CLK2] = PRCM_REG_CORE_CLK2_FADDR;
	prcm_imask_addr[PRCM_REG_CORE_CLK2] = PRCM_REG_CORE_CLK2_IADDR;

	prcm_fmask_mask[PRCM_REG_CORE_CLK3] = PRCM_REG_CORE_CLK3_FMASK;
	prcm_imask_mask[PRCM_REG_CORE_CLK3] = PRCM_REG_CORE_CLK3_IMASK;
	prcm_fmask_addr[PRCM_REG_CORE_CLK3] = PRCM_REG_CORE_CLK3_FADDR;
	prcm_imask_addr[PRCM_REG_CORE_CLK3] = PRCM_REG_CORE_CLK3_IADDR;

	prcm_fmask_mask[PRCM_REG_USBHOST] = PRCM_REG_USBHOST_FMASK;
	prcm_imask_mask[PRCM_REG_USBHOST] = PRCM_REG_USBHOST_IMASK;
	prcm_fmask_addr[PRCM_REG_USBHOST] = PRCM_REG_USBHOST_FADDR;
	prcm_imask_addr[PRCM_REG_USBHOST] = PRCM_REG_USBHOST_IADDR;

}

void
prcm_setclock(int clock, int speed)
{
#if 1
	u_int32_t oreg, reg, mask;
	if (clock == 1) {
		oreg = bus_space_read_4(prcm_iot, prcm_ioh, CM_CLKSEL_WKUP);
		mask = 1;
		reg = (oreg &~mask) | (speed & mask);
		printf(" prcm_setclock old %08x new %08x",  oreg, reg );
		bus_space_write_4(prcm_iot, prcm_ioh, CM_CLKSEL_WKUP, reg);
	} else if (clock >= 2 && clock <= 9) {
		int shift =  (clock-2);
		oreg = bus_space_read_4(prcm_iot, prcm_ioh, CM_CLKSEL_PER);

		mask = 1 << (mask);
		reg =  (oreg & ~mask) | ( (speed << shift) & mask);
		printf(" prcm_setclock old %08x new %08x",  oreg, reg);

		bus_space_write_4(prcm_iot, prcm_ioh, CM_CLKSEL_PER, reg);
	} else
		panic("prcm_setclock invalid clock %d", clock);
#endif
}

void
prcm_enableclock(int bit)
{
	u_int32_t fclk, iclk, fmask, imask, mbit;
	int freg, ireg, reg;
	printf("prcm_enableclock %d:", bit);

	reg = bit >> 5;

	freg = prcm_fmask_addr[reg];
	ireg = prcm_imask_addr[reg];
	fmask = prcm_fmask_mask[reg];
	imask = prcm_imask_mask[reg];

	mbit =  1 << (bit & 0x1f);
#if 0
	printf("reg %d faddr 0x%08x iaddr 0x%08x, fmask 0x%08x "
	    "imask 0x%08x mbit %x", reg, freg, ireg, fmask, imask, mbit);
#endif
	if (fmask & mbit) { /* dont access the register if bit isn't present */
		fclk = bus_space_read_4(prcm_iot, prcm_ioh, freg);
		prcm_fmask_cur[reg] = fclk | mbit;
		bus_space_write_4(prcm_iot, prcm_ioh, freg, fclk | mbit);
		printf(" fclk %08x %08x",  fclk, fclk | mbit);
	}

	if (imask & mbit) { /* dont access the register if bit isn't present */
		iclk = bus_space_read_4(prcm_iot, prcm_ioh, ireg);
		prcm_imask_cur[reg] = iclk | mbit;
		bus_space_write_4(prcm_iot, prcm_ioh, ireg, iclk | mbit);
		printf(" iclk %08x %08x",  iclk, iclk | mbit);
	}
	printf ("\n");
}
