/*	$OpenBSD: bugcrt.c,v 1.3 1998/08/22 09:16:47 smurph Exp $ */
#include <sys/types.h>
#include <machine/prom.h>

struct mvmeprom_args bugargs = { 1 };		/* not BSS */

	asm (".text");
	asm (".long 0x003ffff8");
	asm (".long _start");
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
asm	("#	enable SFU1");
asm	("		ldcr	r25,cr1");
asm	("		xor	r25,r25,0x8");
asm	("		stcr	r25,cr1");

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

	bzero(&edata, (&end - &edata)); 
	id = mvmeprom_brdid();
	bugargs.cputyp = id->model;

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

