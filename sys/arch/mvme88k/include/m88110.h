/*	$OpenBSD: m88110.h,v 1.25 2011/10/25 18:38:06 miod Exp $ */

#ifndef	_MACHINE_M88110_H_
#define	_MACHINE_M88110_H_

/*
 *	88110 CMMU definitions
 */

#define	CMMU_ICMD_INV_ITIC	0x001	/* Invalidate Inst Cache & TIC */
#define	CMMU_ICMD_INV_TIC	0x002	/* Invalidate TIC */
#define	CMMU_ICMD_INV_LINE	0x005	/* Invalidate Inst Cache Line */
#define	CMMU_ICMD_PRB_SUPR	0x008	/* MMU Probe Supervisor */
#define	CMMU_ICMD_PRB_USER	0x009	/* MMU Probe User */
#define	CMMU_ICMD_INV_SATC	0x00a	/* Invalidate All Supervisor ATCs */
#define	CMMU_ICMD_INV_UATC	0x00b	/* Invalidate All User ATCs */

#define	CMMU_ICTL_DID		0x8000	/* Double instruction disable */
#define	CMMU_ICTL_PREN		0x4000	/* Branch Prediction Enable */
#define	CMMU_ICTL_FRZ0		0x0100	/* Inst Cache Freeze Bank 0 */
#define	CMMU_ICTL_FRZ1		0x0080	/* Inst Cache Freeze Bank 1 */
#define	CMMU_ICTL_HTEN		0x0040	/* Hardware Table Search Enable */
#define	CMMU_ICTL_MEN		0x0020	/* Inst MMU Enable */
#define	CMMU_ICTL_BEN		0x0004	/* TIC Cache Enable */
#define	CMMU_ICTL_CEN		0x0001	/* Inst Cache Enable */

#define	CMMU_ISR_TBE		0x200000 /* Table Search Bus Error */
#define	CMMU_ISR_SI		0x100000 /* Segment Fault*/
#define	CMMU_ISR_PI		0x080000 /* Page Fault */
#define	CMMU_ISR_SP		0x040000 /* Supervisor Protection Violation */
#define	CMMU_ISR_PH		0x000800 /* PATC Hit */
#define	CMMU_ISR_BH		0x000400 /* BATC Hit */
#define	CMMU_ISR_SU		0x000200 /* Supervisor Bit */
#define	CMMU_ISR_BE		0x000001 /* Bus Error */

#define	CMMU_DCMD_WB_PG		0x000	/* Flush Data Cache Page (sync) */
#define	CMMU_DCMD_INV_ALL	0x001	/* Invalidate Data Cache All */
#define	CMMU_DCMD_WB_ALL	0x002	/* Flush Data Cache All (sync) */
#define	CMMU_DCMD_WBINV_ALL	0x003	/* Flush Data Cache All (sync + inv) */
#define	CMMU_DCMD_WBINV_PG	0x004	/* Flush Data Cache Page (sync + inv) */
#define	CMMU_DCMD_INV_LINE	0x005	/* Invalidate Data Cache Line */
#define	CMMU_DCMD_WB_LINE	0x006	/* Flush Data Cache Line (sync) */
#define	CMMU_DCMD_WBINV_LINE	0x007	/* Flush Data Cache Line (sync + inv) */
#define	CMMU_DCMD_PRB_SUPR	0x008	/* MMU Probe Supervisor */
#define	CMMU_DCMD_PRB_USER	0x009	/* MMU Probe User */
#define	CMMU_DCMD_INV_SATC	0x00a	/* Invalidate All Supervisor ATCs */
#define	CMMU_DCMD_INV_UATC	0x00b	/* Invalidate All User ATCs */

#define	CMMU_DCTL_RSVD7		0x40000	/* Reserved */
#define	CMMU_DCTL_RSVD6		0x20000	/* Reserved */
#define	CMMU_DCTL_RSVD5		0x10000	/* Reserved */
#define	CMMU_DCTL_RSVD4		0x8000	/* Reserved */
#define	CMMU_DCTL_RSVD3		0x4000	/* Reserved */
#define	CMMU_DCTL_XMEM		0x2000	/* store -> load sequence */
#define	CMMU_DCTL_DEN		0x1000	/* Decoupled Cache Access Enable */
#define	CMMU_DCTL_FWT		0x0800	/* Force Write Through */
#define	CMMU_DCTL_BPEN1		0x0400	/* Break Point Enable 1 */
#define	CMMU_DCTL_BPEN0		0x0200	/* Break Point Enable 0 */
#define	CMMU_DCTL_FRZ0		0x0100	/* Data Cache Freeze Bank 0 */
#define	CMMU_DCTL_FRZ1		0x0080	/* Data Cache Freeze Bank 1 */
#define	CMMU_DCTL_HTEN		0x0040	/* Hardware Table Search Enable */
#define	CMMU_DCTL_MEN		0x0020	/* Data MMU Enable */
#define	CMMU_DCTL_RSVD2		0x0010	/* Reserved */
#define	CMMU_DCTL_ADS		0x0008	/* Allocate Disable */
#define	CMMU_DCTL_RSVD1		0x0004	/* Reserved */
#define	CMMU_DCTL_SEN		0x0002	/* Data Cache Snoop Enable */
#define	CMMU_DCTL_CEN		0x0001	/* Data Cache Enable */

#define	CMMU_DSR_TBE		0x200000 /* Table Search Bus Error */
#define	CMMU_DSR_SI		0x100000 /* Segment Fault */
#define	CMMU_DSR_PI		0x080000 /* Page Fault */
#define	CMMU_DSR_SP		0x040000 /* Supervisor Protection Violation */
#define	CMMU_DSR_WE		0x020000 /* Write Protection Violation */
#define	CMMU_DSR_BPE		0x010000 /* Break Point Exception */
#define	CMMU_DSR_PH		0x000800 /* PATC Hit */
#define	CMMU_DSR_BH		0x000400 /* BATC Hit */
#define	CMMU_DSR_SU		0x000200 /* Supervisor Bit */
#define	CMMU_DSR_RW		0x000100 /* Read Bit */
#define	CMMU_DSR_CP		0x000004 /* Copyback Error */
#define	CMMU_DSR_WA		0x000002 /* Write-Allocate Bus Error */
#define	CMMU_DSR_BE		0x000001 /* Bus Error */

/* definitions for use of the BATC */
#define	BATC_512K		(0x00 << BATC_BLKSHIFT)
#define	BATC_1M			(0x01 << BATC_BLKSHIFT)
#define	BATC_2M			(0x03 << BATC_BLKSHIFT)
#define	BATC_4M			(0x07 << BATC_BLKSHIFT)
#define	BATC_8M			(0x0f << BATC_BLKSHIFT)
#define	BATC_16M		(0x1f << BATC_BLKSHIFT)
#define	BATC_32M		(0x3f << BATC_BLKSHIFT)
#define	BATC_64M		(0x7f << BATC_BLKSHIFT)

/*
 * PATC fields
 */

#define	PATC_VA_MASK		0xfffff000
#define	PATC_SO			0x00000001

/*
 * Cache line information
 */
#define	MC88110_CACHE_SHIFT	5
#define	MC88110_CACHE_LINE	(1 << MC88110_CACHE_SHIFT)

#ifndef	_LOCORE

void	 set_icmd(uint32_t);
void	 set_ictl(uint32_t);
void	 set_isar(uint32_t);
void	 set_isap(uint32_t);
void	 set_iuap(uint32_t);
void	 set_iir(uint32_t);
void	 set_ibp(uint32_t);
void	 set_ippu(uint32_t);
void	 set_ippl(uint32_t);
void	 set_isr(uint32_t);
void	 set_dcmd(uint32_t);
void	 set_dctl(uint32_t);
void	 set_dsar(uint32_t);
void	 set_dsap(uint32_t);
void	 set_duap(uint32_t);
void	 set_dir(uint32_t);
void	 set_dbp(uint32_t);
void	 set_dppu(uint32_t);
void	 set_dppl(uint32_t);
void	 set_dsr(uint32_t);

uint32_t get_dctl(void);
uint32_t get_dsr(void);
uint32_t get_ictl(void);
uint32_t get_isr(void);

/*
 * The following inlines expect their address to be line-aligned for line
 * operations, and page aligned for page operations.
 */

static __inline__ void
mc88110_wb_data_line(paddr_t x)
{
	set_dsar(x);
	set_dcmd(CMMU_DCMD_WB_LINE);
}

static __inline__ void
mc88110_wb_data_page(paddr_t x)
{
	set_dsar(x);
	set_dcmd(CMMU_DCMD_WB_PG);
}

static __inline__ void
mc88110_wb_data(void)
{
	set_dcmd(CMMU_DCMD_WB_ALL);
}

static __inline__ void
mc88110_inval_data_line(paddr_t x)
{
	set_dsar(x);
	set_dcmd(CMMU_DCMD_INV_LINE);
}

static __inline__ void
mc88110_inval_data(void)
{
	set_dcmd(CMMU_DCMD_INV_ALL);
}

static __inline__ void
mc88110_wbinv_data_line(paddr_t x)
{
	set_dsar(x);
	set_dcmd(CMMU_DCMD_WBINV_LINE);
}

static __inline__ void
mc88110_wbinv_data_page(paddr_t x)
{
	set_dsar(x);
	set_dcmd(CMMU_DCMD_WBINV_PG);
}

static __inline__ void
mc88110_wbinv_data(void)
{
	set_dcmd(CMMU_DCMD_WBINV_ALL);
}

static __inline__ void
mc88110_inval_inst_line(paddr_t x)
{
	set_isar(x);
	set_icmd(CMMU_ICMD_INV_LINE);
}

static __inline__ void
mc88110_inval_inst(void)
{
	set_icmd(CMMU_ICMD_INV_ITIC);
}

/* skip one instruction */
static __inline__ void
m88110_skip_insn(struct trapframe *frame)
{
	if (frame->tf_exip & 1)
		frame->tf_exip = frame->tf_enip;
	else
		frame->tf_exip += 4;
}

#endif	/* _LOCORE */
#endif	/* _MACHINE_M88110_H_ */
