/*	$NetBSD: db_interface.c,v 1.6 1995/04/09 11:51:10 pk Exp $ */

/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 *
 *	From: db_interface.c,v 2.4 1991/02/05 17:11:13 mrt (CMU)
 */

/*
 * Interface to new debugger.
 */
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/systm.h> /* just for boothowto --eichin */
#include <setjmp.h>

#include <vm/vm.h>

#include <machine/db_machdep.h>
#include <ddb/db_command.h>
#include <ddb/db_variables.h>
#include <machine/bsd_openprom.h>
#include <machine/ctlreg.h>
#include <sparc/sparc/asm.h>

static int nil;

struct db_variable db_regs[] = {
	{ "psr", (int *)&DDB_TF->tf_psr, FCN_NULL, },
	{ "pc", (int *)&DDB_TF->tf_pc, FCN_NULL, },
	{ "npc", (int *)&DDB_TF->tf_npc, FCN_NULL, },
	{ "y", (int *)&DDB_TF->tf_y, FCN_NULL, },
	{ "wim", (int *)&DDB_TF->tf_global[0], FCN_NULL, }, /* see reg.h */
	{ "g0", (int *)&nil, FCN_NULL, },
	{ "g1", (int *)&DDB_TF->tf_global[1], FCN_NULL, },
	{ "g2", (int *)&DDB_TF->tf_global[2], FCN_NULL, },
	{ "g3", (int *)&DDB_TF->tf_global[3], FCN_NULL, },
	{ "g4", (int *)&DDB_TF->tf_global[4], FCN_NULL, },
	{ "g5", (int *)&DDB_TF->tf_global[5], FCN_NULL, },
	{ "g6", (int *)&DDB_TF->tf_global[6], FCN_NULL, },
	{ "g7", (int *)&DDB_TF->tf_global[7], FCN_NULL, },
	{ "o0", (int *)&DDB_TF->tf_out[0], FCN_NULL, },
	{ "o1", (int *)&DDB_TF->tf_out[1], FCN_NULL, },
	{ "o2", (int *)&DDB_TF->tf_out[2], FCN_NULL, },
	{ "o3", (int *)&DDB_TF->tf_out[3], FCN_NULL, },
	{ "o4", (int *)&DDB_TF->tf_out[4], FCN_NULL, },
	{ "o5", (int *)&DDB_TF->tf_out[5], FCN_NULL, },
	{ "o6", (int *)&DDB_TF->tf_out[6], FCN_NULL, },
	{ "o7", (int *)&DDB_TF->tf_out[7], FCN_NULL, },
	{ "l0", (int *)&DDB_FR->fr_local[0], FCN_NULL, },
	{ "l1", (int *)&DDB_FR->fr_local[1], FCN_NULL, },
	{ "l2", (int *)&DDB_FR->fr_local[2], FCN_NULL, },
	{ "l3", (int *)&DDB_FR->fr_local[3], FCN_NULL, },
	{ "l4", (int *)&DDB_FR->fr_local[4], FCN_NULL, },
	{ "l5", (int *)&DDB_FR->fr_local[5], FCN_NULL, },
	{ "l6", (int *)&DDB_FR->fr_local[6], FCN_NULL, },
	{ "l7", (int *)&DDB_FR->fr_local[7], FCN_NULL, },
	{ "i0", (int *)&DDB_FR->fr_arg[0], FCN_NULL, },
	{ "i1", (int *)&DDB_FR->fr_arg[1], FCN_NULL, },
	{ "i2", (int *)&DDB_FR->fr_arg[2], FCN_NULL, },
	{ "i3", (int *)&DDB_FR->fr_arg[3], FCN_NULL, },
	{ "i4", (int *)&DDB_FR->fr_arg[4], FCN_NULL, },
	{ "i5", (int *)&DDB_FR->fr_arg[5], FCN_NULL, },
	{ "i6", (int *)&DDB_FR->fr_arg[6], FCN_NULL, },
	{ "i7", (int *)&DDB_FR->fr_arg[7], FCN_NULL, },
};
struct db_variable *db_eregs = db_regs + sizeof(db_regs)/sizeof(db_regs[0]);

extern jmp_buf	*db_recover;

int	db_active = 0;

extern char *trap_type[];

/*
 * Received keyboard interrupt sequence.
 */
kdb_kbd_trap(tf)
	struct trapframe *tf;
{
	if (db_active == 0 && (boothowto & RB_KDB)) {
		printf("\n\nkernel: keyboard interrupt\n");
		kdb_trap(-1, 0, tf);
	}
}

/*
 *  kdb_trap - field a TRACE or BPT trap
 */
kdb_trap(type, tf)
	int	type;
	register struct trapframe *tf;
{

	fb_unblank();

	switch (type) {
	case T_BREAKPOINT:	/* breakpoint */
	case -1:		/* keyboard interrupt */
		break;
	default:
		printf("kernel: %s trap", trap_type[type & 0xff]);
		if (db_recover != 0) {
			db_error("Faulted in DDB; continuing...\n");
			/*NOTREACHED*/
		}
	}

	/* Should switch to kdb`s own stack here. */

	ddb_regs.ddb_tf = *tf;
	ddb_regs.ddb_fr = *(struct frame *)tf->tf_out[6];

	db_active++;
	cnpollc(TRUE);
	db_trap(type, 0/*code*/);
	cnpollc(FALSE);
	db_active--;

	*(struct frame *)tf->tf_out[6] = ddb_regs.ddb_fr;
	*tf = ddb_regs.ddb_tf;

	return (1);
}

/*
 * Read bytes from kernel address space for debugger.
 */
void
db_read_bytes(addr, size, data)
	vm_offset_t	addr;
	register int	size;
	register char	*data;
{
	register char	*src;

	src = (char *)addr;
	while (--size >= 0)
		*data++ = *src++;
}


/*
 * XXX - stolen from pmap.c
 */
#define	getpte(va)		lda(va, ASI_PTE)
#define	setpte(va, pte)		sta(va, ASI_PTE, pte)
#define	splpmap() splimp()

static void
db_write_text(dst, ch)
	unsigned char *dst;
	int ch;
{        
	int s, pte0, pte;
	vm_offset_t va;

	s = splpmap();
	va = (unsigned long)dst & (~PGOFSET);
	pte0 = getpte(va);

	if ((pte0 & PG_V) == 0) { 
		db_printf(" address 0x%x not a valid page\n", dst);
		splx(s);
		return;
	}

	pte = pte0 | PG_W;
	setpte(va, pte);

	*dst = (unsigned char)ch;

	setpte(va, pte0);
	splx(s);
}

/*
 * Write bytes to kernel address space for debugger.
 */
void
db_write_bytes(addr, size, data)
	vm_offset_t	addr;
	register int	size;
	register char	*data;
{
	extern char	etext[];
	register char	*dst;

	dst = (char *)addr;
	while (--size >= 0) {
		if ((dst >= (char *)VM_MIN_KERNEL_ADDRESS) && (dst < etext))
			db_write_text(dst, *data);
		else
			*dst = *data;
		dst++, data++;
	}

}

int
Debugger()
{
	asm("ta 0x81");
}

void
db_prom_cmd()
{
	extern struct promvec *promvec;
	promvec->pv_abort();
}

struct db_command sparc_db_command_table[] = {
	{ "prom",	db_prom_cmd,	0,	0 },
	{ (char *)0, }
};

void
db_machine_init()
{
	db_machine_commands_install(sparc_db_command_table);
}
