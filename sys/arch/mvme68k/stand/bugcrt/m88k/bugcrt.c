#include <sys/types.h>
#include <machine/prom.h>

struct bugargs bugargs;
	asm (".text");
	asm (".long 0x003ffff8");
	asm (".long _start");
extern int edata;
extern int end;
start()
{
	register int dev_lun  asm ("r2");
	register int ctrl_lun asm ("r3");
	register int flags asm ("r4");
	register int ctrl_addr asm ("r5");
	register int entry asm ("r6");
	register int conf_blk asm ("r7");
	register char *arg_start asm ("r8");
	register char *arg_end asm ("r9");

	register struct mvmeprom_args *bugarea;

	bugarea = &bugargs;
	bugarea->dev_lun = dev_lun;
	bugarea->ctrl_lun = ctrl_lun;
	bugarea->flags = flags;
	bugarea->ctrl_addr = ctrl_addr;
	bugarea->entry = entry;
	bugarea->conf_blk = conf_blk;
	bugarea->arg_start = arg_start;
	bugarea->arg_end = arg_end;
	*arg_end = 0;

	bzero(&edata, (int)&edata - (int)&end);
	main(bugarea);
	mvmeprom_return();
	/* NOTREACHED */
}

__main()
{
}
