/*	$OpenBSD: crt.c,v 1.5 2003/10/02 13:24:39 miod Exp $ */

#include <sys/types.h>
#include <machine/prom.h>

struct mvmeprom_args bugargs = { 1 };	       /* not BSS */

asm (".text");
/* pseudo reset vector */
asm (STACK_ASM_OP);	/* initial sp value */
asm (".long _start");	/* initial ip value */

void
start()
{
	extern int edata, end;
	struct mvmeprom_brdid *id, *mvmeprom_brdid();

	/* 
	 * This code enables the SFU1 and is used for single stage 
	 * bootstraps or the first stage of a two stage bootstrap.
	 * Do not use r10 to enable the SFU1. This wipes out
	 * the netboot args.  Not cool at all... r25 seems free. 
	 */
	asm("|	enable SFU1");
	asm("	ldcr	r25,cr1" ::: "r25");
	asm("	clr	r25,r25,1<3>"); /* bit 3 is SFU1D */
	asm("	stcr	r25,cr1");

	__asm__ __volatile__ ("or %0, r0, " MVMEPROM_REG_DEVLUN :
	    "=r" (bugargs.dev_lun));
	__asm__ __volatile__ ("or %0, r0, " MVMEPROM_REG_CTRLLUN :
	    "=r" (bugargs.ctrl_lun));
	__asm__ __volatile__ ("or %0, r0, " MVMEPROM_REG_FLAGS :
	    "=r" (bugargs.flags));
	__asm__ __volatile__ ("or %0, r0, " MVMEPROM_REG_CTRLADDR :
	    "=r" (bugargs.ctrl_addr));
	__asm__ __volatile__ ("or %0, r0, " MVMEPROM_REG_ENTRY :
	    "=r" (bugargs.entry));
	__asm__ __volatile__ ("or %0, r0, " MVMEPROM_REG_CONFBLK :
	    "=r" (bugargs.conf_blk));
	__asm__ __volatile__ ("or %0, r0, " MVMEPROM_REG_ARGSTART :
	    "=r" (bugargs.arg_start));
	__asm__ __volatile__ ("or %0, r0, " MVMEPROM_REG_ARGEND :
	    "=r" (bugargs.arg_end));
	__asm__ __volatile__ ("or %0, r0, " MVMEPROM_REG_NBARGSTART :
	    "=r" (bugargs.nbarg_start));
	__asm__ __volatile__ ("or %0, r0, " MVMEPROM_REG_NBARGEND :
	    "=r" (bugargs.nbarg_end));
	*bugargs.arg_end = 0;      

	id = mvmeprom_brdid();
	bugargs.cputyp = id->model;

#ifdef notyet /* STAGE1 */
	/* 
	 * Initialize PSR and CMMU to a known, stable state. 
	 * This has to be done early for MVME197.
	 * Per EB162 mc88110 engineering bulletin.
	 */
	if (bugargs.cputyp == 0x197) {
		asm("|	init MVME197");
		asm("|	1. PSR");
		asm("or.u   r2,r0,0xA200");
		asm("or     r2,r2,0x03E2");
		asm("stcr   r2,cr1");
		asm("|	2. ICTL");
		asm("or     r2,r0,r0");
		asm("or     r2,r2,0x8000");
		asm("or     r2,r2,0x0040");
		asm("stcr   r2,cr26");
		asm("|	3. DCTL");
		asm("or     r2,r0,r0");
		asm("or     r2,r2,0x2000");
		asm("or     r2,r2,0x0040");
		asm("stcr   r2,cr41");
		asm("|	4. init cache");
		asm("or     r2,r0,0x01");
		asm("stcr   r2,cr25");
		asm("stcr   r2,cr40");
	}
#endif

	memset(&edata, 0, ((int)&end - (int)&edata));

	asm  ("|	main()");
	main();
	mvmeprom_return();
	/* NOTREACHED */
}

void
__main()
{
}

void
bugexec(void (*addr)(void))
{
	__asm__ __volatile__ ("or " MVMEPROM_REG_DEVLUN ", r0, %0" ::
	    "r" (bugargs.dev_lun));
	__asm__ __volatile__ ("or " MVMEPROM_REG_CTRLLUN ", r0, %0" ::
	    "r" (bugargs.ctrl_lun));
	__asm__ __volatile__ ("or " MVMEPROM_REG_FLAGS ", r0, %0" ::
	    "r" (bugargs.flags));
	__asm__ __volatile__ ("or " MVMEPROM_REG_CTRLADDR ", r0, %0" ::
	    "r" (bugargs.ctrl_addr));
	__asm__ __volatile__ ("or " MVMEPROM_REG_ENTRY ", r0, %0" ::
	    "r" (bugargs.entry));
	__asm__ __volatile__ ("or " MVMEPROM_REG_CONFBLK ", r0, %0" ::
	    "r" (bugargs.conf_blk));
	__asm__ __volatile__ ("or " MVMEPROM_REG_ARGSTART ", r0, %0" ::
	    "r" (bugargs.arg_start));
	__asm__ __volatile__ ("or " MVMEPROM_REG_ARGEND ", r0, %0" ::
	    "r" (bugargs.arg_end));

	(*addr)();
	printf("bugexec: 0x%x returned!\n", addr);

	_rtt();
}
