#define SPACE_ID 0x01
#define KBUS_SPACE_SHIFT 28
#define KBUS_SLOT_SHIFT	24

#define BID_SLOT	0x0f
#define BID_DIAG	0x10
#define BID_NMI		0x20
#define BID_SYSFAIL	0x40
#define BID_BJMPR0	0x80

#define CPUSTAT 0x4018
#define CPUSTAT_SLAVE	0x01
#define CPUSTAT_GOOD	0x02
#define CPUSTAT_NMI	0x04
#define CPUSTAT_SYSFAIL 0x08

#define DG_CONFIG	0x200
#define DG_HDR_SIZE	16
#define DG_CPU_COUNT	DG_CONFIG
#define DG_MEM_COUNT	(DG_CPU_COUNT + DG_HDR_SIZE)
#define DG_SYS_COUNT	(DG_MEM_COUNT + DG_HDR_SIZE)
#define DG_GPX_COUNT	(DG_SYS_COUNT + DG_HDR_SIZE)
#define DG_COP_COUNT	(DG_GPX_COUNT + DG_HDR_SIZE)
#define DG_TY_COUNT	(DG_COP_COUNT + DG_HDR_SIZE)
#define DG_RES1_COUNT	(DG_TY_COUNT + DG_HDR_SIZE)
#define DG_RES2_COUNT	(DG_RES1_COUNT + DG_HDR_SIZE)
#define DG_RES3_COUNT	(DG_RES2_COUNT + DG_HDR_SIZE)

#define DG_SLOT_INFO	(DG_RES3_COUNT + DG_HDR_SIZE)
#define DG_SLOT_SIZE	8
#define DG_SLOT_SHIFT	3

#define DG_SLOT1	DG_SLOT_INFO
#define DG_SLOT2	(DG_SLOT1 + DG_SLOT_SIZE)
#define DG_SLOT3	(DG_SLOT2 + DG_SLOT_SIZE)
#define DG_SLOT4	(DG_SLOT3 + DG_SLOT_SIZE)
#define DG_SLOT5	(DG_SLOT4 + DG_SLOT_SIZE)
#define DG_SLOT6	(DG_SLOT5 + DG_SLOT_SIZE)
#define DG_SLOT7	(DG_SLOT6 + DG_SLOT_SIZE)
#define DG_SLOT8	(DG_SLOT7 + DG_SLOT_SIZE)
#define DG_SLOT9	(DG_SLOT8 + DG_SLOT_SIZE)
#define DG_SLOTA	(DG_SLOT9 + DG_SLOT_SIZE)
#define DG_SLOTB	(DG_SLOTA + DG_SLOT_SIZE)
#define DG_SLOTC	(DG_SLOTB + DG_SLOT_SIZE)
#define DG_SLOTD	(DG_SLOTC + DG_SLOT_SIZE)
#define DG_SLOTE	(DG_SLOTD + DG_SLOT_SIZE)
#define DG_SLOTF	(DG_SLOTE + DG_SLOT_SIZE)
#define DG_SLOT2	(DG_SLOT1 + DG_SLOT_SIZE)

#define DG_MEM_TOTAL	(DG_SLOTF + DG_SLOT_SIZE)

#define DG_BD_PRESENT	0
#define DG_ID_OFF 1

/* A few things about vme.  */
#define VME_MAP_BASE	0x83000000
#define VME_PAGESIZE	8192
#define VME_MAP_SIZE	2048
#define VME_MAP_SPACE	(VME_PAGESIZE * VME_MAP_SIZE) /* = 2**24 */
#define VME_MAP_PFN	0xffffe000
#define VME_MAP_V	0x1
#define VME_MAP_NV	0x0
#define VME_MAP_A16_MIN	2040
#define VME_MAP_A16_MAX	2047
#define VME16_BASE_USR	0x84ff0000
#define VME16_BASE	0x85ff0000
#define VME16_SIZE	(1 << 16)
#define VME16_MASK	(VME16_SIZE - 1)
#define VME24_BASE_USR	0x86000000
#define VME24_BASE	0x87000000
#define VME24_SIZE	(1 << 24)
#define VME24_MASK	(VME24_SIZE - 1)
#define VME32_BASE_USR	0x80000000
#define VME32_BASE	0x81000000
#define VME32_SIZE	(1 << 24)
#define VME32_MASK	(VME32_SIZE - 1)

/* To ack a vme interruption (and get the vme vector), read a short at
   VME_IACK_BASE + 2 * vme_ipl.  Only the lower byte is significant.
   The ipl is given by the sic interruption (int = 128 + 2 * ipl).  */
#define VME_IACK_BASE	0x82000000
#define VME_IACK_SIZE	0x10

/* The real-time clock.  */
#define RTC_ADDR	0x17020000

/* System board interrupt controller.  */
#define SBIR_BASE	0x17030000	/* SBIR, 1 byte.  */
#define SBIR_DI		0x40		/* 1=directed intr, 0=undirected.  */
#define SBIR_INFO	0x3f		/* For undir intr, information.  */
#define SBIR_DDID	0x3f		/* For dir intr, dest dev id.  */
#define SBI_DIS		0x17030000	/* Read to disable system brd intr.  */
#define SBI_DIS_OFF	0x0
#define SBI_EN		0x17031000	/* Read to enable system brd intr.  */
#define SBI_EN_OFF	0x1000
#define SBIR_SIZE	0x2000
