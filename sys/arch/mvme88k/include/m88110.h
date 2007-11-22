/*	$OpenBSD: m88110.h,v 1.19 2007/11/22 05:53:57 miod Exp $ */

#ifndef	__MACHINE_M88110_H__
#define	__MACHINE_M88110_H__

/*
 *	88110 CMMU definitions
 */
#define CMMU_ICMD 0
#define CMMU_ICTL 1
#define CMMU_ISAR 2
#define CMMU_ISAP 3
#define CMMU_IUAP 4
#define CMMU_IIR  5
#define CMMU_IBP  6
#define CMMU_IPPU 7
#define CMMU_IPPL 8
#define CMMU_ISR  9
#define CMMU_ILAR 10
#define CMMU_IPAR 11

#define CMMU_DCMD 12
#define CMMU_DCTL 13
#define CMMU_DSAR 14
#define CMMU_DSAP 15
#define CMMU_DUAP 16
#define CMMU_DIR  17
#define CMMU_DBP  18
#define CMMU_DPPU 19
#define CMMU_DPPL 20
#define CMMU_DSR  21
#define CMMU_DLAR 22
#define CMMU_DPAR 23

#define CMMU_ICMD_INV_ITIC       0x001    /* Invalidate Inst Cache & TIC */
#define CMMU_ICMD_INV_TIC        0x002    /* Invalidate TIC */
#define CMMU_ICMD_INV_LINE       0x005    /* Invalidate Inst Cache Line */
#define CMMU_ICMD_PRB_SUPR       0x008    /* MMU Probe Supervisor */
#define CMMU_ICMD_PRB_USER       0x009    /* MMU Probe User */
#define CMMU_ICMD_INV_SATC       0x00a    /* Invalidate All Supervisor ATCs */
#define CMMU_ICMD_INV_UATC       0x00b    /* Invalidate All User ATCs */

#define CMMU_ICTL_DID            0x8000   /* Double instruction disable */
#define CMMU_ICTL_PREN           0x4000   /* Branch Prediction Enable */
#define CMMU_ICTL_FRZ0           0x0100   /* Inst Cache Freeze Bank 0 */
#define CMMU_ICTL_FRZ1           0x0080   /* Inst Cache Freeze Bank 1 */
#define CMMU_ICTL_HTEN           0x0040   /* Hardware Table Search Enable */
#define CMMU_ICTL_MEN            0x0020   /* Inst MMU Enable */
#define CMMU_ICTL_BEN            0x0004   /* TIC Cache Enable */
#define CMMU_ICTL_CEN            0x0001   /* Inst Cache Enable */

#define CMMU_ISR_TBE             0x200000 /* Table Search Bus Error */
#define CMMU_ISR_SI              0x100000 /* Segment Fault*/
#define CMMU_ISR_PI              0x080000 /* Page Fault */
#define CMMU_ISR_SP              0x040000 /* Supervisor Protection Violation */
#define CMMU_ISR_PH              0x000800 /* PATC Hit */
#define CMMU_ISR_BH              0x000400 /* BATC Hit */
#define CMMU_ISR_SU              0x000200 /* Supervisor Bit */
#define CMMU_ISR_BE              0x000001 /* Bus Error */

#define CMMU_DCMD_FLUSH_PG       0x000    /* Flush Data Cache Page (sync) */
#define CMMU_DCMD_INV_ALL        0x001    /* Invalidate Data Cache All */
#define CMMU_DCMD_FLUSH_ALL      0x002    /* Flush Data Cache All (sync) */
#define CMMU_DCMD_FLUSH_ALL_INV  0x003    /* Flush Data Cache All (sync & inval) */
#define CMMU_DCMD_FLUSH_PG_INV   0x004    /* Flush Data Cache Page (sync & inval) */
#define CMMU_DCMD_INV_LINE       0x005    /* Invalidate Data Cache Line */
#define CMMU_DCMD_FLUSH_LINE     0x006    /* Flush Data Cache Line (sync)*/
#define CMMU_DCMD_FLUSH_LINE_INV 0x007    /* Flush Data Cache Line (sync & inval)*/
#define CMMU_DCMD_PRB_SUPR       0x008    /* MMU Probe Supervisor */
#define CMMU_DCMD_PRB_USER       0x009    /* MMU Probe User */
#define CMMU_DCMD_INV_SATC       0x00A    /* Invalidate All Supervisor ATCs */
#define CMMU_DCMD_INV_UATC       0x00B    /* Invalidate All User ATCs */

#define CMMU_DCTL_RSVD7          0x40000   /* Reserved */
#define CMMU_DCTL_RSVD6          0x20000   /* Reserved */
#define CMMU_DCTL_RSVD5          0x10000   /* Reserved */
#define CMMU_DCTL_RSVD4          0x8000   /* Reserved */
#define CMMU_DCTL_RSVD3          0x4000   /* Reserved */
#define CMMU_DCTL_XMEM           0x2000   /* store -> load sequence */
#define CMMU_DCTL_DEN            0x1000   /* Decoupled Cache Access Enable */
#define CMMU_DCTL_FWT            0x0800   /* Force Write Through */
#define CMMU_DCTL_BPEN1          0x0400   /* Break Point Enable 1 */
#define CMMU_DCTL_BPEN0          0x0200   /* Break Point Enable 0 */
#define CMMU_DCTL_FRZ0           0x0100   /* Data Cache Freeze Bank 0 */
#define CMMU_DCTL_FRZ1           0x0080   /* Data Cache Freeze Bank 1 */
#define CMMU_DCTL_HTEN           0x0040   /* Hardware Table Search Enable */
#define CMMU_DCTL_MEN            0x0020   /* Data MMU Enable */
#define CMMU_DCTL_RSVD2          0x0010   /* Reserved */
#define CMMU_DCTL_ADS            0x0008   /* Allocat Disable */
#define CMMU_DCTL_RSVD1          0x0004   /* Reserved */
#define CMMU_DCTL_SEN            0x0002   /* Data Cache Snoop Enable */
#define CMMU_DCTL_CEN            0x0001   /* Data Cache Enable */

#define CMMU_DSR_TBE             0x200000 /* Table Search Bus Error */
#define CMMU_DSR_SI              0x100000 /* Segment Fault*/
#define CMMU_DSR_PI              0x080000 /* Page Fault */
#define CMMU_DSR_SP              0x040000 /* Supervisor Protection Violation */
#define CMMU_DSR_WE              0x020000 /* Write Protection Violation */
#define CMMU_DSR_BPE             0x010000 /* Break Point Exception */
#define CMMU_DSR_PH              0x000800 /* PATC Hit */
#define CMMU_DSR_BH              0x000400 /* BATC Hit */
#define CMMU_DSR_SU              0x000200 /* Supervisor Bit */
#define CMMU_DSR_RW              0x000100 /* Read Bit */
#define CMMU_DSR_CP              0x000004 /* Copyback Error */
#define CMMU_DSR_WA              0x000002 /* Write-Allocate Bus Error */
#define CMMU_DSR_BE              0x000001 /* Bus Error */

#define CMMU_READ 0
#define CMMU_WRITE 1
#define CMMU_DATA 1
#define CMMU_INST 0

/* definitions for use of the BATC */
#define BATC_512K	(0x00 << BATC_BLKSHIFT)
#define BATC_1M		(0x01 << BATC_BLKSHIFT)
#define BATC_2M		(0x03 << BATC_BLKSHIFT)
#define BATC_4M		(0x07 << BATC_BLKSHIFT)
#define BATC_8M		(0x0f << BATC_BLKSHIFT)
#define BATC_16M	(0x1f << BATC_BLKSHIFT)
#define BATC_32M	(0x3f << BATC_BLKSHIFT)
#define BATC_64M	(0x7f << BATC_BLKSHIFT)

#define CLINE_MASK	0x1f
#define CLINE_SIZE	(8 * 32)

#ifndef	_LOCORE

void set_icmd(u_int value);
void set_ictl(u_int value);
void set_isar(u_int value);
void set_isap(u_int value);
void set_iuap(u_int value);
void set_iir(u_int value);
void set_ibp(u_int value);
void set_ippu(u_int value);
void set_ippl(u_int value);
void set_isr(u_int value);
void set_dcmd(u_int value);
void set_dctl(u_int value);
void set_dsar(u_int value);
void set_dsap(u_int value);
void set_duap(u_int value);
void set_dir(u_int value);
void set_dbp(u_int value);
void set_dppu(u_int value);
void set_dppl(u_int value);
void set_dsr(u_int value);

/* get routines */
u_int get_icmd(void);
u_int get_ictl(void);
u_int get_isar(void);
u_int get_isap(void);
u_int get_iuap(void);
u_int get_iir(void);
u_int get_ibp(void);
u_int get_ippu(void);
u_int get_ippl(void);
u_int get_isr(void);
u_int get_dcmd(void);
u_int get_dctl(void);
u_int get_dsar(void);
u_int get_dsap(void);
u_int get_duap(void);
u_int get_dir(void);
u_int get_dbp(void);
u_int get_dppu(void);
u_int get_dppl(void);
u_int get_dsr(void);

/* Cache inlines */

#define line_addr(x)	(paddr_t)((x) & ~CLINE_MASK)
#define page_addr(x)	(paddr_t)((x) & ~PAGE_MASK)

/*
 * 88110 general information #22:
 * ``Issuing a command to flush and invalidate the data cache while the
 *   dcache is disabled (CEN = 0 in dctl) will cause problems.  Do not
 *   flush a disabled data cache.  In general, there is no reason to
 *   perform this operation with the cache disabled, since it may be
 *   incoherent with the proper state of memory.  Before 5.0 the flush
 *   command was treated like a nop when the cache was disabled.  This
 *   is no longer the case.''
 */

static __inline__ void mc88110_flush_data_line(paddr_t x)
{
	u_int dctl = get_dctl();
	if (dctl & CMMU_DCTL_CEN) {
		set_dsar(line_addr(x));
		set_dcmd(CMMU_DCMD_FLUSH_LINE);
	}
}

static __inline__ void mc88110_flush_data_page(paddr_t x)
{
	u_int dctl = get_dctl();
	if (dctl & CMMU_DCTL_CEN) {
		set_dsar(page_addr(x));
		set_dcmd(CMMU_DCMD_FLUSH_PG);
	}
}

static __inline__ void mc88110_flush_data(void)
{
	u_int dctl = get_dctl();
	if (dctl & CMMU_DCTL_CEN) {
		set_dcmd(CMMU_DCMD_FLUSH_ALL);
	}
}

static __inline__ void mc88110_inval_data_line(paddr_t x)
{
	set_dsar(line_addr(x));
	set_dcmd(CMMU_DCMD_INV_LINE);
}

static __inline__ void mc88110_inval_data(void)
{
	set_dcmd(CMMU_DCMD_INV_ALL);
}

static __inline__ void mc88110_sync_data_line(paddr_t x)
{
	u_int dctl = get_dctl();
	if (dctl & CMMU_DCTL_CEN) {
		set_dsar(line_addr(x));
		set_dcmd(CMMU_DCMD_FLUSH_LINE_INV);
	}
}

static __inline__ void mc88110_sync_data_page(paddr_t x)
{
	u_int dctl = get_dctl();
	if (dctl & CMMU_DCTL_CEN) {
		set_dsar(page_addr(x));
		set_dcmd(CMMU_DCMD_FLUSH_PG_INV);
	}
}

static __inline__ void mc88110_sync_data(void)
{
	u_int dctl = get_dctl();
	if (dctl & CMMU_DCTL_CEN) {
		set_dcmd(CMMU_DCMD_FLUSH_ALL_INV);
	}
}

static __inline__ void mc88110_inval_inst_line(paddr_t x)
{
	set_isar(line_addr(x));
	set_icmd(CMMU_ICMD_INV_LINE);
}

static __inline__ void mc88110_inval_inst(void)
{
	set_icmd(CMMU_ICMD_INV_ITIC);
}

#endif	/* _LOCORE */
#endif	/* __MACHINE_M88110_H__ */
