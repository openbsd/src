#include <machine/asm.h>

#ifdef MVME197
/* set routines */
ENTRY(set_icmd)
	jmp.n	r1
	stcr	r2, ICMD
ENTRY(set_ictl)
	jmp.n	r1
	stcr	r2, ICTL
ENTRY(set_isar)
	jmp.n	r1
	stcr	r2, ISAR
ENTRY(set_isap)
	FLUSH_PIPELINE
	stcr	r2, ISAP
	jmp	r1
ENTRY(set_iuap)
	FLUSH_PIPELINE
 	stcr	r2, IUAP
	jmp	r1
ENTRY(set_iir)
	jmp.n	r1
	stcr	r2, IIR 
ENTRY(set_ibp)
	jmp.n	r1
	stcr	r2, IBP 
ENTRY(set_ippu)
	jmp.n	r1
	stcr	r2, IPPU
ENTRY(set_ippl)
	jmp.n	r1
	stcr	r2, IPPL
ENTRY(set_isr)
	jmp.n	r1
	stcr	r2, ISR 
ENTRY(set_ilar)
	jmp.n	r1
	stcr	r2, ILAR
ENTRY(set_ipar)
	jmp.n	r1
	stcr	r2, IPAR
ENTRY(set_dcmd)
	jmp.n	r1
	stcr	r2, DCMD
ENTRY(set_dctl)
	jmp.n	r1
	stcr	r2, DCTL
ENTRY(set_dsar)
	stcr	r2, DSAR
	NOP
	jmp	r1
ENTRY(set_dsap)
	FLUSH_PIPELINE
	NOP
	stcr	r2, DSAP
	jmp	r1
ENTRY(set_duap)
	FLUSH_PIPELINE
	NOP
	stcr	r2, DUAP
	jmp	r1
ENTRY(set_dir)
	jmp.n	r1
	stcr	r2, DIR 
ENTRY(set_dbp)
	jmp.n	r1
	stcr	r2, DBP 
ENTRY(set_dppu)
	jmp.n	r1
	stcr	r2, DPPU
ENTRY(set_dppl)
	jmp.n	r1
	stcr	r2, DPPL
ENTRY(set_dsr)
	jmp.n	r1
	stcr	r2, DSR 
ENTRY(set_dlar)
	jmp.n	r1
	stcr	r2, DLAR
ENTRY(set_dpar)
	jmp.n	r1
	stcr	r2, DPAR

/* get routines */
ENTRY(get_icmd)
	jmp.n	r1
	ldcr	r2, ICMD
ENTRY(get_ictl)
	jmp.n	r1
	ldcr	r2, ICTL
ENTRY(get_isar)
	jmp.n	r1
	ldcr	r2, ISAR
ENTRY(get_isap)
	jmp.n	r1
	ldcr	r2, ISAP
ENTRY(get_iuap)
	jmp.n	r1
 	ldcr	r2, IUAP
ENTRY(get_iir)
	jmp.n	r1
	ldcr	r2, IIR 
ENTRY(get_ibp)
	jmp.n	r1
	ldcr	r2, IBP 
ENTRY(get_ippu)
	jmp.n	r1
	ldcr	r2, IPPU
ENTRY(get_ippl)
	jmp.n	r1
	ldcr	r2, IPPL
ENTRY(get_isr)
	jmp.n	r1
	ldcr	r2, ISR 
ENTRY(get_ilar)
	jmp.n	r1
	ldcr	r2, ILAR
ENTRY(get_ipar)
	jmp.n	r1
	ldcr	r2, IPAR
ENTRY(get_dcmd)
	jmp.n	r1
	ldcr	r2, DCMD
ENTRY(get_dctl)
	jmp.n	r1
	ldcr	r2, DCTL
ENTRY(get_dsar)
	jmp.n	r1
	ldcr	r2, DSAR
ENTRY(get_dsap)
	jmp.n	r1
	ldcr	r2, DSAP
ENTRY(get_duap)
	jmp.n	r1
	ldcr	r2, DUAP
ENTRY(get_dir)
	jmp.n	r1
	ldcr	r2, DIR 
ENTRY(get_dbp)
	jmp.n	r1
	ldcr	r2, DBP 
ENTRY(get_dppu)
	jmp.n	r1
	ldcr	r2, DPPU
ENTRY(get_dppl)
	jmp.n	r1
	ldcr	r2, DPPL
ENTRY(get_dsr)
	jmp.n	r1
	ldcr	r2, DSR 
ENTRY(get_dlar)
	jmp.n	r1
	ldcr	r2, DLAR
ENTRY(get_dpar)
	jmp.n	r1
	ldcr	r2, DPAR
#endif /* MVME197 */

