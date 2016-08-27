/* Public Domain */


/*
 * Clocks Signals
 */

#define H3_PLL_PERIPH0		9

#define H3_CLK_APB2		18

#define H3_CLK_BUS_MMC0		22
#define H3_CLK_BUS_MMC1		23
#define H3_CLK_BUS_MMC2		24

#define H3_CLK_BUS_EHCI0	33
#define H3_CLK_BUS_EHCI1	34
#define H3_CLK_BUS_EHCI2	35
#define H3_CLK_BUS_EHCI3	36
#define H3_CLK_BUS_OHCI0	37
#define H3_CLK_BUS_OHCI1	38
#define H3_CLK_BUS_OHCI2	39
#define H3_CLK_BUS_OHCI3	40

#define H3_CLK_BUS_UART0	62
#define H3_CLK_BUS_UART1	63
#define H3_CLK_BUS_UART2	64
#define H3_CLK_BUS_UART3	65

#define H3_CLK_USB_PHY0		88
#define H3_CLK_USB_PHY1		89
#define H3_CLK_USB_PHY2		90
#define H3_CLK_USB_PHY3		91

struct sxiccmu_ccu_bit sun8i_h3_gates[] = {
	[H3_CLK_BUS_MMC0] = { 0x0060, 8 },
	[H3_CLK_BUS_MMC1] = { 0x0060, 9 },
	[H3_CLK_BUS_MMC2] = { 0x0060, 10 },
	[H3_CLK_BUS_EHCI0] = { 0x0060, 24 },
	[H3_CLK_BUS_EHCI1] = { 0x0060, 25 },
	[H3_CLK_BUS_EHCI2] = { 0x0060, 26 },
	[H3_CLK_BUS_EHCI3] = { 0x0060, 27 },
	[H3_CLK_BUS_OHCI0] = { 0x0060, 28 },
	[H3_CLK_BUS_OHCI1] = { 0x0060, 29 },
	[H3_CLK_BUS_OHCI2] = { 0x0060, 30 },
	[H3_CLK_BUS_OHCI3] = { 0x0060, 31 },
	[H3_CLK_BUS_UART0] = { 0x006c, 16, H3_CLK_APB2 },
	[H3_CLK_BUS_UART1] = { 0x006c, 17, H3_CLK_APB2 },
	[H3_CLK_BUS_UART2] = { 0x006c, 18, H3_CLK_APB2 },
	[H3_CLK_BUS_UART3] = { 0x006c, 19, H3_CLK_APB2 },
	[H3_CLK_USB_PHY0] = { 0x00cc, 8 },
	[H3_CLK_USB_PHY1] = { 0x00cc, 9 },
	[H3_CLK_USB_PHY2] = { 0x00cc, 10 },
	[H3_CLK_USB_PHY3] = { 0x00cc, 11 },
};

/*
 * Reset Signals
 */

#define H3_RST_USB_PHY0		0
#define H3_RST_USB_PHY1		1
#define H3_RST_USB_PHY2		2
#define H3_RST_USB_PHY3		3

#define H3_RST_BUS_EHCI0	18
#define H3_RST_BUS_EHCI1	19
#define H3_RST_BUS_EHCI2	20
#define H3_RST_BUS_EHCI3	21
#define H3_RST_BUS_OHCI0	22
#define H3_RST_BUS_OHCI1	23
#define H3_RST_BUS_OHCI2	24
#define H3_RST_BUS_OHCI3	25

struct sxiccmu_ccu_bit sun8i_h3_resets[] = {
	[H3_RST_USB_PHY0] =  { 0x00cc, 0 },
	[H3_RST_USB_PHY1] =  { 0x00cc, 1 },
	[H3_RST_USB_PHY2] =  { 0x00cc, 2 },
	[H3_RST_USB_PHY3] =  { 0x00cc, 3 },
	[H3_RST_BUS_EHCI0] = { 0x02c0, 24 },
	[H3_RST_BUS_EHCI1] = { 0x02c0, 25 },
	[H3_RST_BUS_EHCI2] = { 0x02c0, 26 },
	[H3_RST_BUS_EHCI3] = { 0x02c0, 27 },
	[H3_RST_BUS_OHCI0] = { 0x02c0, 28 },
	[H3_RST_BUS_OHCI1] = { 0x02c0, 29 },
	[H3_RST_BUS_OHCI2] = { 0x02c0, 30 },
	[H3_RST_BUS_OHCI3] = { 0x02c0, 31 },
};
