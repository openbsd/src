/*	$OpenBSD: bugcrt.c,v 1.1 1996/05/10 18:39:15 deraadt Exp $ */

#include <sys/types.h>
#include <machine/prom.h>

struct mvmeprom_args bugargs = { 1 };	/* not in BSS */

	asm (".text");
	asm (".long _start-0x10");
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
	extern int edata, end;

	bugargs.dev_lun = dev_lun;
	bugargs.ctrl_lun = ctrl_lun;
	bugargs.flags = flags;
	bugargs.ctrl_addr = ctrl_addr;
	bugargs.entry = entry;
	bugargs.conf_blk = conf_blk;
	bugargs.arg_start = arg_start;
	bugargs.arg_end = arg_end;
	*arg_end = 0;

	bzero(&edata, (int)&end-(int)&edata);
	main();
	mvmeprom_return();
	/* NOTREACHED */
}

__main()
{
}
