/*	$OpenBSD: m88410.h,v 1.3 2001/12/21 05:53:38 smurph Exp $ */

#ifndef	__MACHINE_M88410_H__
#define	__MACHINE_M88410_H__

/*
 *	mc88410 External Cache Controller definitions
 *	This is only available on MVME197DP/SP models.
 */

#ifndef	_LOCORE

#include <machine/asm_macro.h>
#include <machine/psl.h>
#ifdef _KERNEL
#include <mvme88k/dev/busswreg.h>
#endif 

#define XCC_NOP		"0x0"
#define XCC_FLUSH_PAGE	"0x1"
#define XCC_FLUSH_ALL	"0x2"
#define XCC_INVAL_ALL	"0x3"
#define XCC_ADDR	0xFF800000

static __inline__ void mc88410_flush_page(vm_offset_t physaddr)
{
	vm_offset_t xccaddr = XCC_ADDR | (physaddr >> PGSHIFT);
        m88k_psr_type psr;
	struct bussw_reg *bs = (struct bussw_reg *)BS_BASE;
        u_short	bs_gcsr = bs->bs_gcsr;
	u_short	bs_romcr = bs->bs_romcr;
	
	psr = get_psr();
	/* mask misaligned exceptions */
	set_psr(psr | PSR_MXM);
	/* clear WEN0 and WEN1 in ROMCR (disables writes to FLASH) */
	bs->bs_romcr &= ~(BS_ROMCR_WEN0 | BS_ROMCR_WEN0) ;
	/* set XCC bit in GCSR (0xFF8xxxxx now decodes to mc88410) */
	bs->bs_gcsr |= BS_GCSR_XCC;

	/* load the value of upper32 into r2 */
	__asm__ __volatile__("or   r2,r0," XCC_FLUSH_PAGE);
	/* load the value of lower32 into r3 (always 0) */
	__asm__ __volatile__("or   r3,r0,r0");
	/* load the value of xccaddr into r4 */
	__asm__ __volatile__("or.u r5,r0,hi16(%0)" : : "r" (xccaddr));
	__asm__ __volatile__("ld   r4,r5,lo16(%0)" : : "r" (xccaddr));
	/* make the double write. bang! */
	__asm__ __volatile__("st.d r2,r4,0");
        
	/* spin until the operation starts */
	while (!bs->bs_xccr & BS_XCC_FBSY)
		;
	
	/* restore PSR and friends */
        set_psr(psr);
	flush_pipeline();
        bs->bs_gcsr = bs_gcsr;
	bs->bs_romcr = bs_romcr;
}

static __inline__ void mc88410_flush(void)
{
        m88k_psr_type psr;
	struct bussw_reg *bs = (struct bussw_reg *)BS_BASE;
        u_short	bs_gcsr = bs->bs_gcsr;
	u_short	bs_romcr = bs->bs_romcr;
	
	psr = get_psr();
	/* mask misaligned exceptions */
	set_psr(psr | PSR_MXM);
	/* clear WEN0 and WEN1 in ROMCR (disables writes to FLASH) */
	bs->bs_romcr &= ~(BS_ROMCR_WEN0 | BS_ROMCR_WEN0) ;
	/* set XCC bit in GCSR (0xFF8xxxxx now decodes to mc88410) */
	bs->bs_gcsr |= BS_GCSR_XCC;

	/* load the value of upper32 into r2 */
	__asm__ __volatile__("or   r2,r0," XCC_FLUSH_ALL);
	/* load the value of lower32 into r3 (always 0) */
	__asm__ __volatile__("or   r3,r0,r0");
	/* load the value of xccaddr into r4 */
	__asm__ __volatile__("or.u r5,r0,hi16(0xFF800000)");
	__asm__ __volatile__("or   r4,r5,r0");	/* r4 is now 0xFF800000 */
	/* make the double write. bang! */
	__asm__ __volatile__("st.d r2,r4,0");		
        
	/* spin until the operation starts */
	while (!bs->bs_xccr & BS_XCC_FBSY)
		;
	
	/* restore PSR and friends */
        set_psr(psr);
	flush_pipeline();
        bs->bs_gcsr = bs_gcsr;
	bs->bs_romcr = bs_romcr;
}

static __inline__ void mc88410_inval(void)
{
        m88k_psr_type psr;
	struct bussw_reg *bs = (struct bussw_reg *)BS_BASE;
        u_short	bs_gcsr = bs->bs_gcsr;
	u_short	bs_romcr = bs->bs_romcr;
	
	psr = get_psr();
	/* mask misaligned exceptions */
	set_psr(psr | PSR_MXM);
	/* clear WEN0 and WEN1 in ROMCR (disables writes to FLASH) */
	bs->bs_romcr &= ~(BS_ROMCR_WEN0 | BS_ROMCR_WEN0) ;
	/* set XCC bit in GCSR (0xFF8xxxxx now decodes to mc88410) */
	bs->bs_gcsr |= BS_GCSR_XCC;

	/* load the value of upper32 into r2 */
	__asm__ __volatile__("or   r2,r0," XCC_INVAL_ALL);
	/* load the value of lower32 into r3 (always 0) */
	__asm__ __volatile__("or   r3,r0,r0");
	/* load the value of xccaddr into r4 */
	__asm__ __volatile__("or.u r5,r0,hi16(0xFF800000)");
	__asm__ __volatile__("or   r4,r5,r0");	/* r4 is now 0xFF800000 */
	/* make the double write. bang! */
	__asm__ __volatile__("st.d r2,r4,0");		
        
	/* spin until the operation starts */
	while (!bs->bs_xccr & BS_XCC_FBSY)
		;
	
	/* restore PSR and friends */
        set_psr(psr);
	flush_pipeline();
        bs->bs_gcsr = bs_gcsr;
	bs->bs_romcr = bs_romcr;
}

static __inline__ void mc88410_sync(void)
{
	mc88410_flush();
	mc88410_inval();	
}

#endif	/* _LOCORE */

#endif __MACHINE_M88410_H__
