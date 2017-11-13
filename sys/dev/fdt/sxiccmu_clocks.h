/* Public Domain */


/*
 * Clocks Signals
 */

#define A64_CLK_PLL_PERIPH0	11
#define A64_CLK_PLL_PERIPH0_2X	12

#define A64_CLK_AXI		22
#define A64_CLK_APB		23
#define A64_CLK_AHB1		24
#define A64_CLK_APB1		25
#define A64_CLK_APB2		26
#define A64_CLK_AHB2		27

#define A64_CLK_BUS_MMC0	31
#define A64_CLK_BUS_MMC1	32
#define A64_CLK_BUS_MMC2	33

#define A64_CLK_BUS_EMAC	36

#define A64_CLK_BUS_EHCI0	42
#define A64_CLK_BUS_EHCI1	43
#define A64_CLK_BUS_OHCI0	44
#define A64_CLK_BUS_OHCI1	45

#define A64_CLK_BUS_PIO		58

#define A64_CLK_BUS_I2C0	63
#define A64_CLK_BUS_I2C1	64
#define A64_CLK_BUS_I2C2	65

#define A64_CLK_BUS_UART0	67
#define A64_CLK_BUS_UART1	68
#define A64_CLK_BUS_UART2	69
#define A64_CLK_BUS_UART3	70
#define A64_CLK_BUS_UART4	71

#define A64_CLK_MMC0		75
#define A64_CLK_MMC1		76
#define A64_CLK_MMC2		77

#define A64_CLK_USB_OHCI0	91
#define A64_CLK_USB_OHCI1	93
#define A64_CLK_USB_PHY0	86
#define A64_CLK_USB_PHY1	87

#define A64_CLK_LOSC		254
#define A64_CLK_HOSC		253

struct sxiccmu_ccu_bit sun50i_a64_gates[] = {
	[A64_CLK_BUS_MMC0] =  { 0x0060, 8 },
	[A64_CLK_BUS_MMC1] =  { 0x0060, 9 },
	[A64_CLK_BUS_MMC2] =  { 0x0060, 10 },
	[A64_CLK_BUS_EMAC] =  { 0x0060, 17, A64_CLK_AHB2 },
	[A64_CLK_BUS_EHCI0] = { 0x0060, 24 },
	[A64_CLK_BUS_EHCI1] = { 0x0060, 25 },
	[A64_CLK_BUS_OHCI0] = { 0x0060, 28 },
	[A64_CLK_BUS_OHCI1] = { 0x0060, 29 },
	[A64_CLK_BUS_PIO]   = { 0x0068, 5 },
	[A64_CLK_BUS_I2C0]  = { 0x006c, 0, A64_CLK_APB2 },
	[A64_CLK_BUS_I2C1]  = { 0x006c, 1, A64_CLK_APB2 },
	[A64_CLK_BUS_I2C2]  = { 0x006c, 2, A64_CLK_APB2 },
	[A64_CLK_BUS_UART0] = { 0x006c, 16, A64_CLK_APB2 },
	[A64_CLK_BUS_UART1] = { 0x006c, 17, A64_CLK_APB2 },
	[A64_CLK_BUS_UART2] = { 0x006c, 18, A64_CLK_APB2 },
	[A64_CLK_BUS_UART3] = { 0x006c, 19, A64_CLK_APB2 },
	[A64_CLK_BUS_UART4] = { 0x006c, 20, A64_CLK_APB2 },
	[A64_CLK_MMC0]      = { 0x0088, 31 },
	[A64_CLK_MMC1]      = { 0x008c, 31 },
	[A64_CLK_MMC2]      = { 0x0090, 31 },
	[A64_CLK_USB_OHCI0] = { 0x00cc, 16 },
	[A64_CLK_USB_OHCI1] = { 0x00cc, 17 },
	[A64_CLK_USB_PHY0] =  { 0x00cc,  8 },
	[A64_CLK_USB_PHY1] =  { 0x00cc,  9 },
};

#define H3_CLK_PLL_PERIPH0	9

#define H3_CLK_AXI		15
#define H3_CLK_AHB1		16
#define H3_CLK_APB1		17
#define H3_CLK_APB2		18
#define H3_CLK_AHB2		19

#define H3_CLK_BUS_MMC0		22
#define H3_CLK_BUS_MMC1		23
#define H3_CLK_BUS_MMC2		24

#define H3_CLK_BUS_EMAC		27

#define H3_CLK_BUS_EHCI0	33
#define H3_CLK_BUS_EHCI1	34
#define H3_CLK_BUS_EHCI2	35
#define H3_CLK_BUS_EHCI3	36
#define H3_CLK_BUS_OHCI0	37
#define H3_CLK_BUS_OHCI1	38
#define H3_CLK_BUS_OHCI2	39
#define H3_CLK_BUS_OHCI3	40

#define H3_CLK_BUS_PIO		54

#define H3_CLK_BUS_I2C0		59
#define H3_CLK_BUS_I2C1		60
#define H3_CLK_BUS_I2C2		61
#define H3_CLK_BUS_UART0	62
#define H3_CLK_BUS_UART1	63
#define H3_CLK_BUS_UART2	64
#define H3_CLK_BUS_UART3	65

#define H3_CLK_BUS_EPHY		67

#define H3_CLK_MMC0		71
#define H3_CLK_MMC1		74
#define H3_CLK_MMC2		77

#define H3_CLK_USB_PHY0		88
#define H3_CLK_USB_PHY1		89
#define H3_CLK_USB_PHY2		90
#define H3_CLK_USB_PHY3		91

#define H3_CLK_LOSC		254
#define H3_CLK_HOSC		253

struct sxiccmu_ccu_bit sun8i_h3_gates[] = {
	[H3_CLK_BUS_MMC0] = { 0x0060, 8 },
	[H3_CLK_BUS_MMC1] = { 0x0060, 9 },
	[H3_CLK_BUS_MMC2] = { 0x0060, 10 },
	[H3_CLK_BUS_EMAC] = { 0x0060, 17, H3_CLK_AHB2 },
	[H3_CLK_BUS_EHCI0] = { 0x0060, 24 },
	[H3_CLK_BUS_EHCI1] = { 0x0060, 25 },
	[H3_CLK_BUS_EHCI2] = { 0x0060, 26 },
	[H3_CLK_BUS_EHCI3] = { 0x0060, 27 },
	[H3_CLK_BUS_OHCI0] = { 0x0060, 28 },
	[H3_CLK_BUS_OHCI1] = { 0x0060, 29 },
	[H3_CLK_BUS_OHCI2] = { 0x0060, 30 },
	[H3_CLK_BUS_OHCI3] = { 0x0060, 31 },
	[H3_CLK_BUS_PIO]   = { 0x0068, 5 },
	[H3_CLK_BUS_I2C0]  = { 0x006c, 0, H3_CLK_APB2 },
	[H3_CLK_BUS_I2C1]  = { 0x006c, 1, H3_CLK_APB2 },
	[H3_CLK_BUS_I2C2]  = { 0x006c, 2, H3_CLK_APB2 },
	[H3_CLK_BUS_UART0] = { 0x006c, 16, H3_CLK_APB2 },
	[H3_CLK_BUS_UART1] = { 0x006c, 17, H3_CLK_APB2 },
	[H3_CLK_BUS_UART2] = { 0x006c, 18, H3_CLK_APB2 },
	[H3_CLK_BUS_UART3] = { 0x006c, 19, H3_CLK_APB2 },
	[H3_CLK_BUS_EPHY]  = { 0x0070, 0 },
	[H3_CLK_MMC0]      = { 0x0088, 31 },
	[H3_CLK_MMC1]      = { 0x008c, 31 },
	[H3_CLK_MMC2]      = { 0x0090, 31 },
	[H3_CLK_USB_PHY0]  = { 0x00cc, 8 },
	[H3_CLK_USB_PHY1]  = { 0x00cc, 9 },
	[H3_CLK_USB_PHY2]  = { 0x00cc, 10 },
	[H3_CLK_USB_PHY3]  = { 0x00cc, 11 },
};

/*
 * Reset Signals
 */

#define A64_RST_USB_PHY0	0
#define A64_RST_USB_PHY1	1

#define A64_RST_BUS_MMC0	8
#define A64_RST_BUS_MMC1	9
#define A64_RST_BUS_MMC2	10

#define A64_RST_BUS_EMAC	13

#define A64_RST_BUS_EHCI0	19
#define A64_RST_BUS_EHCI1	20
#define A64_RST_BUS_OHCI0	21
#define A64_RST_BUS_OHCI1	22

#define A64_RST_BUS_I2C0	42
#define A64_RST_BUS_I2C1	43
#define A64_RST_BUS_I2C2	44

struct sxiccmu_ccu_bit sun50i_a64_resets[] = {
	[A64_RST_USB_PHY0] =  { 0x00cc, 0 },
	[A64_RST_USB_PHY1] =  { 0x00cc, 1 },
	[A64_RST_BUS_MMC0] =  { 0x02c0, 8 },
	[A64_RST_BUS_MMC1] =  { 0x02c0, 9 },
	[A64_RST_BUS_MMC2] =  { 0x02c0, 10 },
	[A64_RST_BUS_EMAC] =  { 0x02c0, 17 },
	[A64_RST_BUS_EHCI0] = { 0x02c0, 24 },
	[A64_RST_BUS_EHCI1] = { 0x02c0, 25 },
	[A64_RST_BUS_OHCI0] = { 0x02c0, 28 },
	[A64_RST_BUS_OHCI1] = { 0x02c0, 29 },
	[A64_RST_BUS_I2C0] =  { 0x02d8, 0 },
	[A64_RST_BUS_I2C1] =  { 0x02d8, 1 },
	[A64_RST_BUS_I2C2] =  { 0x02d8, 2 },
};

#define H3_RST_USB_PHY0		0
#define H3_RST_USB_PHY1		1
#define H3_RST_USB_PHY2		2
#define H3_RST_USB_PHY3		3

#define H3_RST_BUS_MMC0		7
#define H3_RST_BUS_MMC1		8
#define H3_RST_BUS_MMC2		9

#define H3_RST_BUS_EMAC		12

#define H3_RST_BUS_EHCI0	18
#define H3_RST_BUS_EHCI1	19
#define H3_RST_BUS_EHCI2	20
#define H3_RST_BUS_EHCI3	21
#define H3_RST_BUS_OHCI0	22
#define H3_RST_BUS_OHCI1	23
#define H3_RST_BUS_OHCI2	24
#define H3_RST_BUS_OHCI3	25

#define H3_RST_BUS_EPHY		39

#define H3_RST_BUS_I2C0		46
#define H3_RST_BUS_I2C1		47
#define H3_RST_BUS_I2C2		48

struct sxiccmu_ccu_bit sun8i_h3_resets[] = {
	[H3_RST_USB_PHY0] =  { 0x00cc, 0 },
	[H3_RST_USB_PHY1] =  { 0x00cc, 1 },
	[H3_RST_USB_PHY2] =  { 0x00cc, 2 },
	[H3_RST_USB_PHY3] =  { 0x00cc, 3 },
	[H3_RST_BUS_MMC0] =  { 0x02c0, 8 },
	[H3_RST_BUS_MMC1] =  { 0x02c0, 9 },
	[H3_RST_BUS_MMC2] =  { 0x02c0, 10 },
	[H3_RST_BUS_EMAC] =  { 0x02c0, 17 },
	[H3_RST_BUS_EHCI0] = { 0x02c0, 24 },
	[H3_RST_BUS_EHCI1] = { 0x02c0, 25 },
	[H3_RST_BUS_EHCI2] = { 0x02c0, 26 },
	[H3_RST_BUS_EHCI3] = { 0x02c0, 27 },
	[H3_RST_BUS_OHCI0] = { 0x02c0, 28 },
	[H3_RST_BUS_OHCI1] = { 0x02c0, 29 },
	[H3_RST_BUS_OHCI2] = { 0x02c0, 30 },
	[H3_RST_BUS_OHCI3] = { 0x02c0, 31 },
	[H3_RST_BUS_EPHY]  = { 0x02c8, 2 },
	[H3_RST_BUS_I2C0]  = { 0x02d8, 0 },
	[H3_RST_BUS_I2C1]  = { 0x02d8, 1 },
	[H3_RST_BUS_I2C2]  = { 0x02d8, 2 },
};
