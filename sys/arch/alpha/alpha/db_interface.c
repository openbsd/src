/*	$OpenBSD: db_interface.c,v 1.4 1997/07/09 07:57:10 deraadt Exp $	*/

/*
 * Copyright (c) 1997 Niklas Hallqvist.  All rights reserverd.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Niklas Hallqvist.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/systm.h>

#include <machine/db_machdep.h>
#include <machine/frame.h>

#include <ddb/db_access.h>
#include <ddb/db_command.h>
#include <ddb/db_output.h>
#include <ddb/db_sym.h>
#include <ddb/db_variables.h>
#include <ddb/db_extern.h>

#include <dev/cons.h>

extern label_t *db_recover;
extern char    *trap_type[];
extern int	trap_types;

void kdbprinttrap __P((int, int));

struct db_variable db_regs[] = {
	{ "v0", (long *)&ddb_regs.tf_regs[FRAME_V0], FCN_NULL, },
	{ "t0", (long *)&ddb_regs.tf_regs[FRAME_T0], FCN_NULL, },
	{ "t1", (long *)&ddb_regs.tf_regs[FRAME_T1], FCN_NULL, },
	{ "t2", (long *)&ddb_regs.tf_regs[FRAME_T2], FCN_NULL, },
	{ "t3", (long *)&ddb_regs.tf_regs[FRAME_T3], FCN_NULL, },
	{ "t4", (long *)&ddb_regs.tf_regs[FRAME_T4], FCN_NULL, },
	{ "t5", (long *)&ddb_regs.tf_regs[FRAME_T5], FCN_NULL, },
	{ "t6", (long *)&ddb_regs.tf_regs[FRAME_T6], FCN_NULL, },
	{ "t7", (long *)&ddb_regs.tf_regs[FRAME_T7], FCN_NULL, },
	{ "s0", (long *)&ddb_regs.tf_regs[FRAME_S0], FCN_NULL, },
	{ "s1", (long *)&ddb_regs.tf_regs[FRAME_S1], FCN_NULL, },
	{ "s2", (long *)&ddb_regs.tf_regs[FRAME_S2], FCN_NULL, },
	{ "s3", (long *)&ddb_regs.tf_regs[FRAME_S3], FCN_NULL, },
	{ "s4", (long *)&ddb_regs.tf_regs[FRAME_S4], FCN_NULL, },
	{ "s5", (long *)&ddb_regs.tf_regs[FRAME_S5], FCN_NULL, },
	{ "s6", (long *)&ddb_regs.tf_regs[FRAME_S6], FCN_NULL, },
	{ "a0", (long *)&ddb_regs.tf_regs[FRAME_A0], FCN_NULL, },
	{ "a1", (long *)&ddb_regs.tf_regs[FRAME_A1], FCN_NULL, },
	{ "a2", (long *)&ddb_regs.tf_regs[FRAME_A2], FCN_NULL, },
	{ "a3", (long *)&ddb_regs.tf_regs[FRAME_A3], FCN_NULL, },
	{ "a4", (long *)&ddb_regs.tf_regs[FRAME_A4], FCN_NULL, },
	{ "a5", (long *)&ddb_regs.tf_regs[FRAME_A5], FCN_NULL, },
	{ "t8", (long *)&ddb_regs.tf_regs[FRAME_T8], FCN_NULL, },
	{ "t9", (long *)&ddb_regs.tf_regs[FRAME_T9], FCN_NULL, },
	{ "t10", (long *)&ddb_regs.tf_regs[FRAME_T10], FCN_NULL, },
	{ "t11", (long *)&ddb_regs.tf_regs[FRAME_T11], FCN_NULL, },
	{ "ra", (long *)&ddb_regs.tf_regs[FRAME_RA], FCN_NULL, },
	{ "t12", (long *)&ddb_regs.tf_regs[FRAME_T12], FCN_NULL, },
	{ "at", (long *)&ddb_regs.tf_regs[FRAME_AT], FCN_NULL, },
	{ "gp", (long *)&ddb_regs.tf_regs[FRAME_GP], FCN_NULL, },
	{ "sp", (long *)&ddb_regs.tf_regs[FRAME_SP], FCN_NULL, },
	{ "ps", (long *)&ddb_regs.tf_regs[FRAME_PS], FCN_NULL, },
	{ "pc", (long *)&ddb_regs.tf_regs[FRAME_PC], FCN_NULL, },
};

struct db_variable *db_eregs = db_regs + sizeof(db_regs)/sizeof(db_regs[0]);
int	db_active = 0;

void
Debugger()
{
  	__asm__ ("bpt");
}

/*
 * Read bytes from kernel address space for debugger.
 */
void
db_read_bytes(addr, size, data)
	vm_offset_t addr;
	size_t size;
	char *data;
{
	char *src = (char*)addr;

	while (size > 0) {
		--size;
		*data++ = *src++;
	}
}

/*
 * Write bytes to kernel address space for debugger.
 */
void
db_write_bytes(addr, size, data)
	vm_offset_t addr;
	size_t size;
	char *data;
{
	char *dst = (char *)addr;

	while (size > 0) {
		--size;
		*dst++ = *data++;
	}
}

/*
 * Print trap reason.
 */
void
kdbprinttrap(type, code)
	int type, code;
{
	db_printf("kernel: ");
	if (type >= trap_types || type < 0)
		db_printf("type %d", type);
	else
		db_printf("%s", trap_type[type]);
	db_printf(" trap, code=%x\n", code);
}

/*
 *  kdb_trap - field a TRACE or BPT trap
 */
int
kdb_trap(type, code, regs)
	int type, code;
	db_regs_t *regs;
{
	int s;

	switch (type) {
	case ALPHA_IF_CODE_BPT:		/* breakpoint */
	case -1:			/* keyboard interrupt */
		break;
	default:
		kdbprinttrap(type, code);
		if (db_recover != 0) {
			db_error("Faulted in DDB; continuing...\n");
			/*NOTREACHED*/
		}
	}

	/* XXX Should switch to kdb`s own stack here. */

	db_printf("db_regs at %p\n", regs);
	ddb_regs = *regs;
	ddb_regs.tf_regs[FRAME_SP] = (u_long)regs + FRAME_SW_SIZE*8;

	s = splhigh();
	db_active++;
	cnpollc(TRUE);
	db_trap(type, code);
	cnpollc(FALSE);
	db_active--;
	splx(s);

	/* XXX set regs from ddb_regs here */

	return (1);
}
