
/* General constants.  */
#define SIC_NUM_IPL	256	/* Number of interrupt priority levels.  */

/* For DIR.  */
#define SIC_DIR_MASK	0x3f	/* Current device id.  */

/* For IPR.  */
#define SIC_IPR_MASK	0xff	/* Current interrupt priority level.  */
#define SIC_IPR_CLR_MASK 0xffff0000	/* Clear bits always read as 1.  */

/* For IRC.  */
#define SIC_IRC_E	0x00000001	/* Enable receiver.  */
#define SIC_IRC_P	0x00000002	/* Interrupt pending.  */

/* For IPV.  */
#define SIC_IPV_IVL	0xff00		/* Pending interrupt vector level.  */
#define SIC_IPV_BRDCST	0x0080		/* Broadcast interrupt if set.  */
#define SIC_IPV_DIR	0x0040		/* Directed interrupt if set.  */
#define SIC_IPV_INFO	0x003f		/* Misc info if undirected int.  */
#define SIC_IPV_DDID	0x003f		/* Dest dev id if directed int.  */

/* For IXR.  */
#define SIC_IXR_IVL	0xff00		/* Pending interrupt vector level.  */
#define SIC_IXR_DIR	0x0040		/* Directed interrupt if set.  */
#define SIC_IXR_INFO	0x003f		/* Misc info if undirected int.  */
#define SIC_IXR_DDID	0x003f		/* Dest dev id if directed int.  */

/* For ITXC.  */
#define SIC_ITXC_E	0x0001		/* Transmitter enable bit.  */
#define SIC_ITXC_A	0x0002		/* interrupt acked bit.  */

/* Interrupt vectors.  */
#define INTR_VME_1	0x82		/* 130: vme level 1. */
#define INTR_IOASIC	0x83		/* 131: ioasic, wd33c93a.  */
#define INTR_VME_2	0x84		/* 132: vme level 2.  */
#define INTR_FB		0x85		/* 133: frame buffer.  */
#define INTR_VME_3	0x86		/* 134: vme level 3. */
#define INTR_LANCE	0x87		/* 135: LANCE.  */
#define INTR_VME_4	0x88		/* 136: vme level 4. */
#define INTR_ZS		0x89		/* 137: zs.  */
#define INTR_VME_5	0x8a		/* 138: vme level 5. */
#define INTR_VME_6	0x8c		/* 140: vme level 6. */
#define INTR_CLOCK	0x8d		/* 141: clock.  */
#define INTR_VME_7	0x8e		/* 142: vme level 7. */

/* Get a sic intr from a vme level.  */
#define VME_IPL_TO_INTR(l)	(128 + 2 * (l))
