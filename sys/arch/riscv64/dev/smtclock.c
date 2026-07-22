/*	$OpenBSD: smtclock.c,v 1.5 2026/07/22 20:07:22 kettenis Exp $	*/
/*
 * Copyright (c) 2026 Mark Kettenis <kettenis@openbsd.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_clock.h>
#include <dev/ofw/ofw_misc.h>
#include <dev/ofw/fdt.h>

/* K1 APBC clocks */
#define K1_CLK_UART0		0
#define K1_CLK_UART2		1
#define K1_CLK_UART3		2
#define K1_CLK_UART4		3
#define K1_CLK_UART5		4
#define K1_CLK_UART6		5
#define K1_CLK_UART7		6
#define K1_CLK_UART8		7
#define K1_CLK_UART9		8
#define K1_CLK_TWSI0		32
#define K1_CLK_TWSI1		33
#define K1_CLK_TWSI2		34
#define K1_CLK_TWSI4		35
#define K1_CLK_TWSI5		36
#define K1_CLK_TWSI6		37
#define K1_CLK_TWSI7		38
#define K1_CLK_TWSI8		39
#define K1_CLK_AIB		42
#define K1_CLK_TWSI0_BUS	84
#define K1_CLK_TWSI1_BUS	85
#define K1_CLK_TWSI2_BUS	86
#define K1_CLK_TWSI4_BUS	87
#define K1_CLK_TWSI5_BUS	88
#define K1_CLK_TWSI6_BUS	89
#define K1_CLK_TWSI7_BUS	90
#define K1_CLK_TWSI8_BUS	91
#define K1_CLK_AIB_BUS		94

/* K1 APBC resets */
#define K1_RESET_UART0		0
#define K1_RESET_UART2		1
#define K1_RESET_UART3		2
#define K1_RESET_UART4		3
#define K1_RESET_UART5		4
#define K1_RESET_UART6		5
#define K1_RESET_UART7		6
#define K1_RESET_UART8		7
#define K1_RESET_UART9		8
#define K1_RESET_TWSI0		32
#define K1_RESET_TWSI1		33
#define K1_RESET_TWSI2		34
#define K1_RESET_TWSI4		35
#define K1_RESET_TWSI5		36
#define K1_RESET_TWSI6		37
#define K1_RESET_TWSI7		38
#define K1_RESET_TWSI8		39

/* K1 APMU clocks */
#define K1_CLK_USB30		16
#define K1_CLK_PCIE0_MASTER	28
#define K1_CLK_PCIE0_SLAVE	29
#define K1_CLK_PCIE0_DBI	30
#define K1_CLK_PCIE1_MASTER	31
#define K1_CLK_PCIE1_SLAVE	32
#define K1_CLK_PCIE1_DBI	33
#define K1_CLK_PCIE2_MASTER	34
#define K1_CLK_PCIE2_SLAVE	35
#define K1_CLK_PCIE2_DBI	36
#define K1_CLK_EMAC0_BUS	37
#define K1_CLK_EMAC1_BUS	39

/* K1 APMU resets */
#define K1_RESET_USB30_AHB	8
#define K1_RESET_USB30_VCC	9
#define K1_RESET_USB30_PHY	10
#define K1_RESET_PCIE0_MASTER	23
#define K1_RESET_PCIE0_SLAVE	24
#define K1_RESET_PCIE0_DBI	25
#define K1_RESET_PCIE0_GLOBAL	26
#define K1_RESET_PCIE1_MASTER	27
#define K1_RESET_PCIE1_SLAVE	28
#define K1_RESET_PCIE1_DBI	29
#define K1_RESET_PCIE1_GLOBAL	30
#define K1_RESET_PCIE2_MASTER	31
#define K1_RESET_PCIE2_SLAVE	32
#define K1_RESET_PCIE2_DBI	33
#define K1_RESET_PCIE2_GLOBAL	34
#define K1_RESET_EMAC0		35
#define K1_RESET_EMAC1		36

/* K3 APBC clocks */
#define K3_CLK_UART0		0
#define K3_CLK_UART2		1
#define K3_CLK_UART3		2
#define K3_CLK_UART4		3
#define K3_CLK_UART5		4
#define K3_CLK_UART6		5
#define K3_CLK_UART7		6
#define K3_CLK_UART8		7
#define K3_CLK_UART9		8
#define K3_CLK_UART10		9
#define K3_CLK_TWSI0		73
#define K3_CLK_TWSI1		74
#define K3_CLK_TWSI2		75
#define K3_CLK_TWSI4		76
#define K3_CLK_TWSI5		77
#define K3_CLK_TWSI6		78
#define K3_CLK_TWSI8		79
#define K3_CLK_TWSI0_BUS	80
#define K3_CLK_TWSI1_BUS	81
#define K3_CLK_TWSI2_BUS	82
#define K3_CLK_TWSI4_BUS	83
#define K3_CLK_TWSI5_BUS	84
#define K3_CLK_TWSI6_BUS	85
#define K3_CLK_TWSI8_BUS	86
#define K3_CLK_AIB		103
#define K3_CLK_AIB_BUS		104

/* K3 APBC resets */
#define K3_RESET_UART0		0
#define K3_RESET_UART2		1
#define K3_RESET_UART3		2
#define K3_RESET_UART4		3
#define K3_RESET_UART5		4
#define K3_RESET_UART6		5
#define K3_RESET_UART7		6
#define K3_RESET_UART8		7
#define K3_RESET_UART9		8
#define K3_RESET_UART10		9
#define K3_RESET_TWSI0		35
#define K3_RESET_TWSI1		36
#define K3_RESET_TWSI2		37
#define K3_RESET_TWSI4		38
#define K3_RESET_TWSI5		39
#define K3_RESET_TWSI6		40
#define K3_RESET_TWSI8		41

/* K3 APMU clocks */
#define K3_CLK_USB2_BUS	30
#define K3_CLK_EMAC0_BUS	66
#define K3_CLK_EMAC0_RGMII_TX	69
#define K3_CLK_EMAC1_BUS	70
#define K3_CLK_EMAC1_RGMII_TX	73
#define K3_CLK_EMAC2_BUS	74
#define K3_CLK_EMAC2_RGMII_TX	77
#define K3_CLK_AXI		-128

/* K3 APMU resets */
#define K3_RESET_USB2_AHB	16
#define K3_RESET_USB2_VCC	17
#define K3_RESET_USB2_PHY	18
#define K3_RESET_EMAC0		67
#define K3_RESET_EMAC1		68
#define K3_RESET_EMAC2		69

/* APBC registers */
#define APBC_UART0_CLK_RST		0x0000
#define APBC_UART2_CLK_RST		0x0004
#define APBC_TWSI8_CLK_RST		0x0020
#define  APBC_TWSI8_CLK_RST_RST		(1U << 2)
#define  APBC_TWSI8_CLK_RST_FNCLK	(1U << 1)
#define  APBC_TWSI8_CLK_RST_APBCLK	(1U << 0)
#define APBC_UART3_CLK_RST		0x0024
#define APBC_TWSI0_CLK_RST		0x002c
#define APBC_TWSI1_CLK_RST		0x0030
#define APBC_TWSI2_CLK_RST		0x0038
#define APBC_AIB_CLK_RST		0x003c
#define APBC_TWSI4_CLK_RST		0x0040
#define APBC_TWSI5_CLK_RST		0x004c
#define APBC_TWSI6_CLK_RST		0x0060
#define APBC_TWSI7_CLK_RST		0x0068
#define APBC_UART4_CLK_RST		0x0070
#define APBC_UART5_CLK_RST		0x0074
#define APBC_UART6_CLK_RST		0x0078
#define APBC_UART7_CLK_RST		0x0094
#define APBC_UART8_CLK_RST		0x0098
#define APBC_UART9_CLK_RST		0x009c
#define APBC_UART10_CLK_RST		0x0154	/* only on K3 */
#define  APBC_UARTX_CLK_RST_FNCLKSEL(x)	(((x) >> 4) & 0x7)

/* APMU registers */
#define APMU_USB_CLK_RES_CTRL		0x005c
#define APMU_EMAC2_CLK_RST_CTRL		0x0248 /* only on K3 */
#define APMU_ACLK_CTRL			0x0388
#define  APMU_ACLK_CTRL_ACLK_DIV(x)	(((x) >> 1) & 0x3)
#define  APMU_ACLK_CTRL_ACLK_SEL(x)	(((x) >> 0) & 0x1)
#define APMU_PCIE_CLK_RES_CTRL_PORTA	0x03cc
#define APMU_PCIE_CLK_RES_CTRL_PORTB	0x03d4
#define APMU_PCIE_CLK_RES_CTRL_PORTC	0x03dc
#define APMU_EMAC0_CLK_RST_CTRL		0x03e4
#define APMU_EMAC1_CLK_RST_CTRL		0x03ec

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))
#define HSET4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) | (bits))
#define HCLR4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) & ~(bits))

struct smtclock {
	int8_t		idx;
	uint16_t	reg;
	int8_t		bit;
};

struct smtreset {
	int8_t		idx;
	uint16_t	reg;
	int8_t		assert_bit;
	int8_t		deassert_bit;
};

static struct smtclock k1_apbc_clocks[] = {
	{ K1_CLK_UART0, APBC_UART0_CLK_RST, 1 },
	{ K1_CLK_UART2, APBC_UART2_CLK_RST, 1 },
	{ K1_CLK_UART3, APBC_UART3_CLK_RST, 1 },
	{ K1_CLK_UART4, APBC_UART4_CLK_RST, 1 },
	{ K1_CLK_UART5, APBC_UART5_CLK_RST, 1 },
	{ K1_CLK_UART6, APBC_UART6_CLK_RST, 1 },
	{ K1_CLK_UART7, APBC_UART7_CLK_RST, 1 },
	{ K1_CLK_UART8, APBC_UART8_CLK_RST, 1 },
	{ K1_CLK_UART9, APBC_UART9_CLK_RST, 1 },
	{ K1_CLK_TWSI0, APBC_TWSI0_CLK_RST, 1 },
	{ K1_CLK_TWSI1, APBC_TWSI1_CLK_RST, 1 },
	{ K1_CLK_TWSI2, APBC_TWSI2_CLK_RST, 1 },
	{ K1_CLK_TWSI4, APBC_TWSI4_CLK_RST, 1 },
	{ K1_CLK_TWSI5, APBC_TWSI5_CLK_RST, 1 },
	{ K1_CLK_TWSI6, APBC_TWSI6_CLK_RST, 1 },
	{ K1_CLK_TWSI7, APBC_TWSI7_CLK_RST, 1 },
	{ K1_CLK_TWSI8, APBC_TWSI8_CLK_RST, 1 },
	{ K1_CLK_AIB, APBC_AIB_CLK_RST, 1 },
	{ K1_CLK_TWSI0_BUS, APBC_TWSI0_CLK_RST, 0 },
	{ K1_CLK_TWSI1_BUS, APBC_TWSI1_CLK_RST, 0 },
	{ K1_CLK_TWSI2_BUS, APBC_TWSI2_CLK_RST, 0 },
	{ K1_CLK_TWSI4_BUS, APBC_TWSI4_CLK_RST, 0 },
	{ K1_CLK_TWSI5_BUS, APBC_TWSI5_CLK_RST, 0 },
	{ K1_CLK_TWSI6_BUS, APBC_TWSI6_CLK_RST, 0 },
	{ K1_CLK_TWSI7_BUS, APBC_TWSI7_CLK_RST, 0 },
	{ K1_CLK_TWSI8_BUS, APBC_TWSI8_CLK_RST, 0 },
	{ K1_CLK_AIB_BUS, APBC_AIB_CLK_RST, 0 },
	{ -1 },
};

static struct smtreset k1_apbc_resets[] = {
	{ K1_RESET_UART0, APBC_UART0_CLK_RST, 2, -1 },
	{ K1_RESET_UART2, APBC_UART2_CLK_RST, 2, -1 },
	{ K1_RESET_UART3, APBC_UART3_CLK_RST, 2, -1 },
	{ K1_RESET_UART4, APBC_UART4_CLK_RST, 2, -1 },
	{ K1_RESET_UART5, APBC_UART5_CLK_RST, 2, -1 },
	{ K1_RESET_UART6, APBC_UART6_CLK_RST, 2, -1 },
	{ K1_RESET_UART7, APBC_UART7_CLK_RST, 2, -1 },
	{ K1_RESET_UART8, APBC_UART8_CLK_RST, 2, -1 },
	{ K1_RESET_UART9, APBC_UART9_CLK_RST, 2, -1 },
	{ K1_RESET_TWSI0, APBC_TWSI0_CLK_RST, 2, -1 },
	{ K1_RESET_TWSI1, APBC_TWSI1_CLK_RST, 2, -1 },
	{ K1_RESET_TWSI2, APBC_TWSI2_CLK_RST, 2, -1 },
	{ K1_RESET_TWSI4, APBC_TWSI4_CLK_RST, 2, -1 },
	{ K1_RESET_TWSI5, APBC_TWSI5_CLK_RST, 2, -1 },
	{ K1_RESET_TWSI6, APBC_TWSI6_CLK_RST, 2, -1 },
	{ K1_RESET_TWSI7, APBC_TWSI7_CLK_RST, 2, -1 },
	{ K1_RESET_TWSI8, APBC_TWSI8_CLK_RST, 2, -1 },
	{ -1 },
};

static struct smtclock k1_apmu_clocks[] = {
	{ K1_CLK_USB30, APMU_USB_CLK_RES_CTRL, 8 },
	{ K1_CLK_PCIE0_MASTER, APMU_PCIE_CLK_RES_CTRL_PORTA, 2 },
	{ K1_CLK_PCIE0_SLAVE, APMU_PCIE_CLK_RES_CTRL_PORTA, 1 },
	{ K1_CLK_PCIE0_DBI, APMU_PCIE_CLK_RES_CTRL_PORTA, 0 },
	{ K1_CLK_PCIE1_MASTER, APMU_PCIE_CLK_RES_CTRL_PORTB, 2 },
	{ K1_CLK_PCIE1_SLAVE, APMU_PCIE_CLK_RES_CTRL_PORTB, 1 },
	{ K1_CLK_PCIE1_DBI, APMU_PCIE_CLK_RES_CTRL_PORTB, 0 },
	{ K1_CLK_PCIE2_MASTER, APMU_PCIE_CLK_RES_CTRL_PORTC, 2 },
	{ K1_CLK_PCIE2_SLAVE, APMU_PCIE_CLK_RES_CTRL_PORTC, 1 },
	{ K1_CLK_PCIE2_DBI, APMU_PCIE_CLK_RES_CTRL_PORTC, 0 },
	{ K1_CLK_EMAC0_BUS, APMU_EMAC0_CLK_RST_CTRL, 0 },
	{ K1_CLK_EMAC1_BUS, APMU_EMAC1_CLK_RST_CTRL, 0 },
	{ -1 },
};

static struct smtreset k1_apmu_resets[] = {
	{ K1_RESET_USB30_AHB, APMU_USB_CLK_RES_CTRL, -1, 9 },
	{ K1_RESET_USB30_VCC, APMU_USB_CLK_RES_CTRL, -1, 10 },
	{ K1_RESET_USB30_PHY, APMU_USB_CLK_RES_CTRL, -1, 11 },
	{ K1_RESET_PCIE0_MASTER, APMU_PCIE_CLK_RES_CTRL_PORTA, -1, 5 },
	{ K1_RESET_PCIE0_SLAVE, APMU_PCIE_CLK_RES_CTRL_PORTA, -1, 4 },
	{ K1_RESET_PCIE0_DBI, APMU_PCIE_CLK_RES_CTRL_PORTA, -1, 3 },
	{ K1_RESET_PCIE0_GLOBAL, APMU_PCIE_CLK_RES_CTRL_PORTA, 8, -1 },
	{ K1_RESET_PCIE1_MASTER, APMU_PCIE_CLK_RES_CTRL_PORTB, -1, 5 },
	{ K1_RESET_PCIE1_SLAVE, APMU_PCIE_CLK_RES_CTRL_PORTB, -1, 4 },
	{ K1_RESET_PCIE1_DBI, APMU_PCIE_CLK_RES_CTRL_PORTB, -1, 3 },
	{ K1_RESET_PCIE1_GLOBAL, APMU_PCIE_CLK_RES_CTRL_PORTB, 8, -1 },
	{ K1_RESET_PCIE2_MASTER, APMU_PCIE_CLK_RES_CTRL_PORTC, -1, 5 },
	{ K1_RESET_PCIE2_SLAVE, APMU_PCIE_CLK_RES_CTRL_PORTC, -1, 4 },
	{ K1_RESET_PCIE2_DBI, APMU_PCIE_CLK_RES_CTRL_PORTC, -1, 3 },
	{ K1_RESET_PCIE2_GLOBAL, APMU_PCIE_CLK_RES_CTRL_PORTC, 8, -1 },
	{ K1_RESET_EMAC0, APMU_EMAC0_CLK_RST_CTRL, -1, 1 },
	{ K1_RESET_EMAC1, APMU_EMAC1_CLK_RST_CTRL, -1, 1 },
	{ -1 },
};

static struct smtclock k3_apbc_clocks[] = {
	{ K3_CLK_UART0, APBC_UART0_CLK_RST, 1 },
	{ K3_CLK_UART2, APBC_UART2_CLK_RST, 1 },
	{ K3_CLK_UART3, APBC_UART3_CLK_RST, 1 },
	{ K3_CLK_UART4, APBC_UART4_CLK_RST, 1 },
	{ K3_CLK_UART5, APBC_UART5_CLK_RST, 1 },
	{ K3_CLK_UART6, APBC_UART6_CLK_RST, 1 },
	{ K3_CLK_UART7, APBC_UART7_CLK_RST, 1 },
	{ K3_CLK_UART8, APBC_UART8_CLK_RST, 1 },
	{ K3_CLK_UART9, APBC_UART9_CLK_RST, 1 },
	{ K3_CLK_UART10, APBC_UART10_CLK_RST, 1 },
	{ K3_CLK_TWSI0, APBC_TWSI0_CLK_RST, 1 },
	{ K3_CLK_TWSI1, APBC_TWSI1_CLK_RST, 1 },
	{ K3_CLK_TWSI2, APBC_TWSI2_CLK_RST, 1 },
	{ K3_CLK_TWSI4, APBC_TWSI4_CLK_RST, 1 },
	{ K3_CLK_TWSI5, APBC_TWSI5_CLK_RST, 1 },
	{ K3_CLK_TWSI6, APBC_TWSI6_CLK_RST, 1 },
	{ K3_CLK_TWSI8, APBC_TWSI8_CLK_RST, 1 },
	{ K3_CLK_TWSI0_BUS, APBC_TWSI0_CLK_RST, 0 },
	{ K3_CLK_TWSI1_BUS, APBC_TWSI1_CLK_RST, 0 },
	{ K3_CLK_TWSI2_BUS, APBC_TWSI2_CLK_RST, 0 },
	{ K3_CLK_TWSI4_BUS, APBC_TWSI4_CLK_RST, 0 },
	{ K3_CLK_TWSI5_BUS, APBC_TWSI5_CLK_RST, 0 },
	{ K3_CLK_TWSI6_BUS, APBC_TWSI6_CLK_RST, 0 },
	{ K3_CLK_TWSI8_BUS, APBC_TWSI8_CLK_RST, 0 },
	{ K3_CLK_AIB, APBC_AIB_CLK_RST, 1 },
	{ K3_CLK_AIB_BUS, APBC_AIB_CLK_RST, 0 },
	{ -1 },
};

static struct smtreset k3_apbc_resets[] = {
	{ K3_RESET_UART0, APBC_UART0_CLK_RST, 2, -1 },
	{ K3_RESET_UART2, APBC_UART2_CLK_RST, 2, -1 },
	{ K3_RESET_UART3, APBC_UART3_CLK_RST, 2, -1 },
	{ K3_RESET_UART4, APBC_UART4_CLK_RST, 2, -1 },
	{ K3_RESET_UART5, APBC_UART5_CLK_RST, 2, -1 },
	{ K3_RESET_UART6, APBC_UART6_CLK_RST, 2, -1 },
	{ K3_RESET_UART7, APBC_UART7_CLK_RST, 2, -1 },
	{ K3_RESET_UART8, APBC_UART8_CLK_RST, 2, -1 },
	{ K3_RESET_UART9, APBC_UART9_CLK_RST, 2, -1 },
	{ K3_RESET_UART10, APBC_UART10_CLK_RST, 2, -1 },
	{ K3_RESET_TWSI0, APBC_TWSI0_CLK_RST, 2, -1 },
	{ K3_RESET_TWSI1, APBC_TWSI1_CLK_RST, 2, -1 },
	{ K3_RESET_TWSI2, APBC_TWSI2_CLK_RST, 2, -1 },
	{ K3_RESET_TWSI4, APBC_TWSI4_CLK_RST, 2, -1 },
	{ K3_RESET_TWSI5, APBC_TWSI5_CLK_RST, 2, -1 },
	{ K3_RESET_TWSI6, APBC_TWSI6_CLK_RST, 2, -1 },
	{ K3_RESET_TWSI8, APBC_TWSI8_CLK_RST, 2, -1 },
	{ -1 },
};

static struct smtclock k3_apmu_clocks[] = {
	{ K3_CLK_USB2_BUS, APMU_USB_CLK_RES_CTRL, 1 },
	{ K3_CLK_EMAC0_BUS, APMU_EMAC0_CLK_RST_CTRL, 0 },
	{ K3_CLK_EMAC0_RGMII_TX, APMU_EMAC0_CLK_RST_CTRL, 8 },
	{ K3_CLK_EMAC1_BUS, APMU_EMAC1_CLK_RST_CTRL, 0 },
	{ K3_CLK_EMAC1_RGMII_TX, APMU_EMAC1_CLK_RST_CTRL, 8 },
	{ K3_CLK_EMAC2_BUS, APMU_EMAC2_CLK_RST_CTRL, 0 },
	{ K3_CLK_EMAC2_RGMII_TX, APMU_EMAC2_CLK_RST_CTRL, 8 },
	{ K3_CLK_AXI, APMU_ACLK_CTRL, -1 },
	{ -1 },
};

static struct smtreset k3_apmu_resets[] = {
	{ K3_RESET_USB2_AHB, APMU_USB_CLK_RES_CTRL, -1, 0 },
	{ K3_RESET_USB2_VCC, APMU_USB_CLK_RES_CTRL, -1, 2 },
	{ K3_RESET_USB2_PHY, APMU_USB_CLK_RES_CTRL, -1, 3 },
	{ K3_RESET_EMAC0, APMU_EMAC0_CLK_RST_CTRL, -1, 1 },
	{ K3_RESET_EMAC1, APMU_EMAC1_CLK_RST_CTRL, -1, 1 },
	{ K3_RESET_EMAC2, APMU_EMAC2_CLK_RST_CTRL, -1, 1 },
	{ -1 },
};

struct smtclock_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	int			sc_node;

	const struct smtclock	*sc_clocks;
	const struct smtreset	*sc_resets;

	struct clock_device	sc_cd;
	struct reset_device	sc_rd;
};

int	smtclock_match(struct device *, void *, void *);
void	smtclock_attach(struct device *, struct device *, void *);

const struct cfattach smtclock_ca = {
	sizeof (struct smtclock_softc), smtclock_match, smtclock_attach
};

struct cfdriver smtclock_cd = {
	NULL, "smtclock", DV_DULL
};

uint32_t smtclock_get_frequency(void *, uint32_t *);
int	smtclock_set_frequency(void *, uint32_t *, uint32_t);
void	smtclock_enable(void *, uint32_t *, int);
void	smtclock_reset(void *, uint32_t *, int);

int
smtclock_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "spacemit,k1-syscon-apbc") ||
	    OF_is_compatible(faa->fa_node, "spacemit,k1-syscon-apmu") ||
	    OF_is_compatible(faa->fa_node, "spacemit,k3-syscon-apbc") ||
	    OF_is_compatible(faa->fa_node, "spacemit,k3-syscon-apmu");
}

void
smtclock_attach(struct device *parent, struct device *self, void *aux)
{
	struct smtclock_softc *sc = (struct smtclock_softc *)self;
	struct fdt_attach_args *faa = aux;

	if (faa->fa_nreg < 1) {
		printf(": no registers\n");
		return;
	}

	sc->sc_iot = faa->fa_iot;
	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_ioh)) {
		printf(": can't map registers\n");
		return;
	}

	sc->sc_node = faa->fa_node;
	regmap_register(sc->sc_node, sc->sc_iot, sc->sc_ioh,
	    faa->fa_reg[0].size);

	if (OF_is_compatible(faa->fa_node, "spacemit,k1-syscon-apbc")) {
		sc->sc_clocks = k1_apbc_clocks;
		sc->sc_resets = k1_apbc_resets;
	} else if (OF_is_compatible(faa->fa_node, "spacemit,k1-syscon-apmu")) {
		sc->sc_clocks = k1_apmu_clocks;
		sc->sc_resets = k1_apmu_resets;
	} else if (OF_is_compatible(faa->fa_node, "spacemit,k3-syscon-apbc")) {
		sc->sc_clocks = k3_apbc_clocks;
		sc->sc_resets = k3_apbc_resets;
	} else if (OF_is_compatible(faa->fa_node, "spacemit,k3-syscon-apmu")) {
		sc->sc_clocks = k3_apmu_clocks;
		sc->sc_resets = k3_apmu_resets;
	}
	KASSERT(sc->sc_clocks);
	KASSERT(sc->sc_resets);

	printf("\n");

	sc->sc_cd.cd_node = faa->fa_node;
	sc->sc_cd.cd_cookie = sc;
	sc->sc_cd.cd_get_frequency = smtclock_get_frequency;
	sc->sc_cd.cd_set_frequency = smtclock_set_frequency;
	sc->sc_cd.cd_enable = smtclock_enable;
	clock_register(&sc->sc_cd);

	sc->sc_rd.rd_node = faa->fa_node;
	sc->sc_rd.rd_cookie = sc;
	sc->sc_rd.rd_reset = smtclock_reset;
	reset_register(&sc->sc_rd);
}

uint32_t
smtclock_get_frequency(void *cookie, uint32_t *cells)
{
	struct smtclock_softc *sc = cookie;
	const struct smtclock *clock;
	uint32_t idx = cells[0];
	uint32_t parent;
	uint32_t div, reg;

	for (clock = sc->sc_clocks; clock->idx != -1; clock++) {
		if (clock->idx == idx)
			break;
	}

	if (clock->idx == -1)
		goto fail;

	if (OF_is_compatible(sc->sc_node, "spacemit,k1-syscon-apbc")) {
		switch (idx) {
		case K1_CLK_UART0:
		case K1_CLK_UART2:
		case K1_CLK_UART3:
		case K1_CLK_UART4:
		case K1_CLK_UART5:
		case K1_CLK_UART6:
		case K1_CLK_UART7:
		case K1_CLK_UART8:
		case K1_CLK_UART9:
			reg = HREAD4(sc, clock->reg);
			switch (APBC_UARTX_CLK_RST_FNCLKSEL(reg)) {
			case 0:
				return 57600000;
			case 1:
				return 14745600;
			case 2:
				return 48000000;
			}
			break;
		}
	}

	if (OF_is_compatible(sc->sc_node, "spacemit,k3-syscon-apbc")) {
		switch (idx) {
		case K3_CLK_UART0:
		case K3_CLK_UART2:
		case K3_CLK_UART3:
		case K3_CLK_UART4:
		case K3_CLK_UART5:
		case K3_CLK_UART6:
		case K3_CLK_UART7:
		case K3_CLK_UART8:
		case K3_CLK_UART9:
		case K3_CLK_UART10:
			reg = HREAD4(sc, clock->reg);
			switch (APBC_UARTX_CLK_RST_FNCLKSEL(reg)) {
			case 0:
				return 57600000;
			case 1:
				return 14745600;
			case 2:
				return 48000000;
			}
			break;
		}
	}
	if (OF_is_compatible(sc->sc_node, "spacemit,k3-syscon-apmu")) {
		switch (idx) {
		case K3_CLK_AXI:
			reg = HREAD4(sc, clock->reg);
			div = APMU_ACLK_CTRL_ACLK_DIV(reg);
			switch (APMU_ACLK_CTRL_ACLK_SEL(reg)) {
			case 0:
				return 307200000 / (div + 1);
			case 1:
				return 409600000 / (div + 1);
			}
			break;
		case K3_CLK_EMAC0_BUS:
			parent = K3_CLK_AXI;
			return smtclock_get_frequency(sc, &parent);
		}
	}

fail:
	printf("%s: 0x%08x\n", __func__, idx);
	return 0;
}

int
smtclock_set_frequency(void *cookie, uint32_t *cells, uint32_t freq)
{
	uint32_t idx = cells[0];

	printf("%s: 0x%08x\n", __func__, idx);
	return -1;
}

void
smtclock_enable(void *cookie, uint32_t *cells, int on)
{
	struct smtclock_softc *sc = cookie;
	const struct smtclock *clock;
	uint32_t idx = cells[0];

	for (clock = sc->sc_clocks; clock->idx != -1; clock++) {
		if (clock->idx == idx)
			break;
	}

	if (clock->idx == -1 || clock->bit == -1) {
		printf("%s: 0x%08x\n", __func__, idx);
		return;
	}

	if (OF_is_compatible(sc->sc_node, "spacemit,k1-syscon-apbc")) {
		/*
		 * To work around the fact that the APBC_TWSI8_CLK_RST
		 * register is write-only, we enable both clocks and
		 * clear the reset at the same time.
		 */
		switch (idx) {
		case K1_CLK_TWSI8:
		case K1_CLK_TWSI8_BUS:
			if (on)
				HWRITE4(sc, APBC_TWSI8_CLK_RST,
				    APBC_TWSI8_CLK_RST_FNCLK |
				    APBC_TWSI8_CLK_RST_APBCLK);
			else
				HWRITE4(sc, clock->reg, 0);
			return;
		}
	}

	if (on)
		HSET4(sc, clock->reg, (1U << clock->bit));
	else
		HCLR4(sc, clock->reg, (1U << clock->bit));
}

void
smtclock_reset(void *cookie, uint32_t *cells, int assert)
{
	struct smtclock_softc *sc = cookie;
	const struct smtreset *reset;
	uint32_t idx = cells[0];
	uint32_t assert_mask = 0;
	uint32_t deassert_mask = 0;
	uint32_t mask, val;

	for (reset = sc->sc_resets; reset->idx != -1; reset++) {
		if (reset->idx == idx)
			break;
	}

	if (reset->idx == -1) {
		printf("%s: 0x%08x\n", __func__, idx);
		return;
	}

	if (OF_is_compatible(sc->sc_node, "spacemit,k1-syscon-apbc")) {
		/*
		 * To work around the fact that the APBC_TWSI8_CLK_RST
		 * register is write-only, we enable both clocks and
		 * clear the reset at the same time.
		 */
		switch (idx) {
		case K1_RESET_TWSI8:
			if (assert)
				HWRITE4(sc, APBC_TWSI8_CLK_RST,
				    APBC_TWSI8_CLK_RST_RST);
			else
				HWRITE4(sc, APBC_TWSI8_CLK_RST,
				    APBC_TWSI8_CLK_RST_FNCLK |
				    APBC_TWSI8_CLK_RST_APBCLK);
			return;
		}
	}

	if (reset->assert_bit != -1)
		assert_mask = (1U << reset->assert_bit);
	if (reset->deassert_bit != -1)
		deassert_mask = (1U << reset->deassert_bit);

	mask = assert_mask | deassert_mask;
	val = HREAD4(sc, reset->reg) & ~mask;
	if (assert)
		val |= assert_mask;
	else
		val |= deassert_mask;
	HWRITE4(sc, reset->reg, val);
}
