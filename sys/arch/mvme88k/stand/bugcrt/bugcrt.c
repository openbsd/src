/*	$OpenBSD: bugcrt.c,v 1.5 1999/09/27 19:30:00 smurph Exp $ */
#include <sys/types.h>
#include <machine/prom.h>

struct mvmeprom_args bugargs = { 1 };		/* not BSS */

	asm (".text");
	/* pseudo reset vector */
	asm (".long 0x00Af0000"); /* initial sp value */
	asm (".long _start");     /* initial ip value */
start()
{
	register int dev_lun asm (MVMEPROM_REG_DEVLUN);
	register int ctrl_lun asm (MVMEPROM_REG_CTRLLUN);
	register int flags asm (MVMEPROM_REG_FLAGS);
	register int ctrl_addr asm (MVMEPROM_REG_CTRLADDR);
	register int entry asm (MVMEPROM_REG_ENTRY);
	register int conf_blk asm (MVMEPROM_REG_CONFBLK);
	register char *arg_start asm (MVMEPROM_REG_ARGSTART);
	register char *arg_end asm (MVMEPROM_REG_ARGEND);
	register char *nbarg_start asm (MVMEPROM_REG_NBARGSTART);
	register char *nbarg_end asm (MVMEPROM_REG_NBARGEND);
	extern int edata, end;
	struct mvmeprom_brdid *id, *mvmeprom_brdid();

	/* Do not use r10 to enable the SFU1. This wipes out 
	   the netboot args.  Not cool at all... r25 seems free. */
   asm("|	enable SFU1");
   asm("	ldcr	r25,cr1");
   asm("	xor	r25,r25,0x8");
   asm("	stcr	r25,cr1");

	bugargs.dev_lun = dev_lun;
	bugargs.ctrl_lun = ctrl_lun;
	bugargs.flags = flags;
	bugargs.ctrl_addr = ctrl_addr;
	bugargs.entry = entry;
	bugargs.conf_blk = conf_blk;
	bugargs.arg_start = arg_start;
	bugargs.arg_end = arg_end;
	bugargs.nbarg_start = nbarg_start;
	bugargs.nbarg_end = nbarg_end;
	*bugargs.arg_end = 0;
/*
	id = mvmeprom_brdid();
	bugargs.cputyp = id->model;
*/	
   /* 
    * Initialize PSR and CMMU to a known, stable state. 
    * This has to be done early for MVME197.
    * Per EB162 mc88110 engineering bulletin.
    */
   /*
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
   */
   memset(&edata, 0, ((int)&end - (int)&edata));
   
   asm	("|	main()");
	main();
	mvmeprom_return();
	/* NOTREACHED */
}

__main()
{
}

void
bugexec(addr)

void (*addr)();

{
	register int dev_lun asm (MVMEPROM_REG_DEVLUN);
	register int ctrl_lun asm (MVMEPROM_REG_CTRLLUN);
	register int flags asm (MVMEPROM_REG_FLAGS);
	register int ctrl_addr asm (MVMEPROM_REG_CTRLADDR);
	register int entry asm (MVMEPROM_REG_ENTRY);
	register int conf_blk asm (MVMEPROM_REG_CONFBLK);
	register char *arg_start asm (MVMEPROM_REG_ARGSTART);
	register char *arg_end asm (MVMEPROM_REG_ARGEND);

	dev_lun = bugargs.dev_lun;
	ctrl_lun = bugargs.ctrl_lun;
	flags = bugargs.flags;
	ctrl_addr = bugargs.ctrl_addr;
	entry = bugargs.entry;
	conf_blk = bugargs.conf_blk;
	arg_start = bugargs.arg_start;
	arg_end = bugargs.arg_end;

	(*addr)();
	printf("bugexec: 0x%x returned!\n", addr);

	_rtt();
}

