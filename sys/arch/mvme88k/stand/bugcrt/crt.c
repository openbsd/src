/*	$OpenBSD: crt.c,v 1.6 2006/05/16 22:51:30 miod Exp $ */

#include <sys/types.h>
#include <machine/prom.h>

#include "stand.h"
#include "libbug.h"

struct mvmeprom_args bugargs;

__asm__ (".text");
__asm__ (STACK_ASM_OP);		/* initial sp value */
__asm__ (".long _start");	/* initial ip value */

extern void main(void);

void
start(u_int dev_lun, u_int ctrl_lun, u_int flags, u_int ctrl_addr, u_int entry,
    u_int conf_blk, char *arg_start, char *arg_end)
{
	extern u_int edata, end;
	char *nbarg_start;
	char *nbarg_end;
	u_int dummy;

	/*
	 * Save r10 and r11 first. We can't put declare them as arguments
	 * since the normal calling convention would put them on the stack.
	 */
	__asm__ __volatile__ ("or %0, r0, r10" : "=r" (nbarg_start) : :
	    "r10", "r11");
	__asm__ __volatile__ ("or %0, r0, r11" : "=r" (nbarg_end) : :
	    "r10", "r11");

	/*
	 * This code enables the SFU1 and is used for single stage
	 * bootstraps or the first stage of a two stage bootstrap.
	 * Do not use lower registers to enable the SFU1. This wipes out
	 * the args.  Not cool at all... r25 seems free.
	 */
	__asm__ __volatile__ ("ldcr %0, cr1" : "=r" (dummy));
	__asm__ __volatile__ ("clr %0, %0, 1<3>; stcr %0, cr1" : "+r" (dummy));

	memset(&edata, 0, ((int)&end - (int)&edata));

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
	*bugargs.arg_end = '\0';

	main();
	_rtt();
	/* NOTREACHED */
}

void
__main()
{
}

void
bugexec(void (*addr)())
{
	(*addr)(bugargs.dev_lun, bugargs.ctrl_lun, bugargs.flags,
	    bugargs.ctrl_addr, bugargs.entry, bugargs.conf_blk,
	    bugargs.arg_start, bugargs.arg_end);

	printf("bugexec: %p returned!\n", addr);

	_rtt();
}
