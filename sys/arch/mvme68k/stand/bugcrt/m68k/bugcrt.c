/*	$OpenBSD: bugcrt.c,v 1.4 1996/05/07 03:06:15 rahnds Exp $ */

#include <sys/types.h>
#include <machine/prom.h>

struct mvmeprom_args bugargs = { 1 };	/* not in BSS */

	asm (".text");
	asm (".long _start-0x10");
	asm (".long _start");
start()
{
	register int dev_lun asm ("d0");
	register int ctrl_lun asm ("d1");
	register int flags asm ("d4");
	register int ctrl_addr asm ("a0");
	register int entry asm ("a1");
	register int conf_blk asm ("a2");
	register char *arg_start asm ("a5");
	register char *arg_end asm ("a6");
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
