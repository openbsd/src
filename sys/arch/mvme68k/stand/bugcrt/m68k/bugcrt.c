#include <sys/types.h>
#include <machine/prom.h>

struct mvmeprom_args bugargs;

	asm (".text");
	asm (".long 0x003ffff8");
	asm (".long _start");
extern int edata;
extern int end;
start()
{
	register int dev_lun  asm ("d0");
	register int ctrl_lun asm ("d1");
	register int flags asm ("d4");
	register int ctrl_addr asm ("a0");
	register int entry asm ("a1");
	register int conf_blk asm ("a2");
	register char *arg_start asm ("a5");
	register char *arg_end asm ("a6");

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

	bzero(&edata, (int)&end-(int)&edata);
	main(bugarea);
	mvmeprom_return();
	/* NOTREACHED */
}

__main()
{
}
