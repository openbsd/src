#include <sys/types.h>
#include <machine/prom.h>

struct mvmeprom_args bugargs = { 1 };		/* not BSS */

	asm (".text");
	asm (".long 0x003ffff8");
	asm (".long _start");
start()
{
	register int dev_lun asm ("r2");
	register int ctrl_lun asm ("r3");
	register int flags asm ("r4");
	register int ctrl_addr asm ("r5");
	register int entry asm ("r6");
	register int conf_blk asm ("r7");
	register char *arg_start asm ("r8");
	register char *arg_end asm ("r9");
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

	bzero(&edata, (int)&edata - (int)&end);
	main();
	mvmeprom_return();
	/* NOTREACHED */
}

__main()
{
}
