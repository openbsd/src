/*	$OpenBSD: db_interface.c,v 1.2 1999/02/25 19:19:20 mickey Exp $	*/

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>

#include <vm/vm.h>

#include <machine/db_machdep.h>
#include <machine/frame.h>

#include <ddb/db_access.h>
#include <ddb/db_command.h>
#include <ddb/db_output.h>
#include <ddb/db_run.h>
#include <ddb/db_sym.h>
#include <ddb/db_variables.h>
#include <ddb/db_extern.h>
#include <ddb/db_interface.h>

#include <dev/cons.h>

void kdbprinttrap __P((int, int));


extern label_t *db_recover;
extern int db_active;
extern char *trap_type[];
extern int trap_types;

db_regs_t	ddb_regs;
struct db_variable db_regs[] = {
	{ "r1",    (long *)&ddb_regs.r1,  FCN_NULL },
	{ "rp",    (long *)&ddb_regs.rp,  FCN_NULL },
	{ "r3",    (long *)&ddb_regs.r3,  FCN_NULL },
	{ "r4",    (long *)&ddb_regs.r4,  FCN_NULL },
	{ "r5",    (long *)&ddb_regs.r5,  FCN_NULL },
	{ "r6",    (long *)&ddb_regs.r6,  FCN_NULL },
	{ "r7",    (long *)&ddb_regs.r7,  FCN_NULL },
	{ "r8",    (long *)&ddb_regs.r8,  FCN_NULL },
	{ "r9",    (long *)&ddb_regs.r9,  FCN_NULL },
	{ "r10",   (long *)&ddb_regs.r10, FCN_NULL },
	{ "r11",   (long *)&ddb_regs.r11, FCN_NULL },
	{ "r12",   (long *)&ddb_regs.r12, FCN_NULL },
	{ "r13",   (long *)&ddb_regs.r13, FCN_NULL },
	{ "r14",   (long *)&ddb_regs.r14, FCN_NULL },
	{ "r15",   (long *)&ddb_regs.r15, FCN_NULL },
	{ "r16",   (long *)&ddb_regs.r16, FCN_NULL },
	{ "r17",   (long *)&ddb_regs.r17, FCN_NULL },
	{ "r18",   (long *)&ddb_regs.r18, FCN_NULL },
	{ "t4",    (long *)&ddb_regs.t4,  FCN_NULL },
	{ "t3",    (long *)&ddb_regs.t3,  FCN_NULL },
	{ "t2",    (long *)&ddb_regs.t2,  FCN_NULL },
	{ "t1",    (long *)&ddb_regs.t1,  FCN_NULL },
	{ "arg3",  (long *)&ddb_regs.arg3,  FCN_NULL },
	{ "arg2",  (long *)&ddb_regs.arg2,  FCN_NULL },
	{ "arg1",  (long *)&ddb_regs.arg1,  FCN_NULL },
	{ "arg0",  (long *)&ddb_regs.arg0,  FCN_NULL },
	{ "dp",    (long *)&ddb_regs.dp,    FCN_NULL },
	{ "ret0",  (long *)&ddb_regs.ret0,  FCN_NULL },
	{ "ret1",  (long *)&ddb_regs.ret1,  FCN_NULL },
	{ "sp",    (long *)&ddb_regs.sp,    FCN_NULL },
	{ "r31",   (long *)&ddb_regs.r31,   FCN_NULL },
	{ "sar",   (long *)&ddb_regs.sar,   FCN_NULL },

	{ "eiem",  (long *)&ddb_regs.eiem,  FCN_NULL },
	{ "iir",   (long *)&ddb_regs.iir,   FCN_NULL },
	{ "isr",   (long *)&ddb_regs.isr,   FCN_NULL },
	{ "ior",   (long *)&ddb_regs.ior,   FCN_NULL },
	{ "ipsw",  (long *)&ddb_regs.ipsw,  FCN_NULL },

	{ "sr0",   (long *)&ddb_regs.sr0,   FCN_NULL },
	{ "sr1",   (long *)&ddb_regs.sr1,   FCN_NULL },
	{ "sr2",   (long *)&ddb_regs.sr2,   FCN_NULL },
	{ "sr3",   (long *)&ddb_regs.sr3,   FCN_NULL },
	{ "sr4",   (long *)&ddb_regs.sr4,   FCN_NULL },
	{ "sr5",   (long *)&ddb_regs.sr5,   FCN_NULL },
	{ "sr6",   (long *)&ddb_regs.sr6,   FCN_NULL },
	{ "sr7",   (long *)&ddb_regs.sr7,   FCN_NULL },

	{ "pidr1", (long *)&ddb_regs.pidr1, FCN_NULL },
	{ "pidr2", (long *)&ddb_regs.pidr2, FCN_NULL },
	{ "pidr3", (long *)&ddb_regs.pidr3, FCN_NULL },
	{ "pidr4", (long *)&ddb_regs.pidr4, FCN_NULL },

	{ "hptm",  (long *)&ddb_regs.hptm,  FCN_NULL },
	{ "vtop",  (long *)&ddb_regs.vtop,  FCN_NULL },
#if 0
	u_int	ccr;	/* cr10 */
	u_int	tr2;	/* cr26 */
#endif
};
struct db_variable *db_eregs = db_regs + sizeof(db_regs)/sizeof(db_regs[0]);
int db_active = 0;

void
Debugger()
{
	__asm("break 0,0");
}

void
db_read_bytes(addr, size, data)
	vm_offset_t addr;
	size_t size;
	char *data;
{
	register char *src = (char*)addr;

	while (size--)
		*data++ = *src++;
}

void
db_write_bytes(addr, size, data)
	vm_offset_t addr;
	size_t size;
	char *data;
{
	register char *dst = (char *)addr;

	while (size--)
		*dst++ = *data++;
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
 *  kdb_trap - field a BPT trap
 */
int
kdb_trap(type, code, regs)
	int type, code;
	db_regs_t *regs;
{
	int s;

	/* XXX Should switch to kdb`s own stack here. */

	ddb_regs = *regs;

	s = splhigh();
	db_active++;
	cnpollc(TRUE);
	db_trap(type, code);
	cnpollc(FALSE);
	db_active--;
	splx(s);

	*regs = ddb_regs;
	return (1);
}

int
db_valid_breakpoint(addr)
	db_addr_t addr;
{
	return (1);
}

void
db_stack_trace_cmd(addr, have_addr, count, modif)
	db_expr_t	addr;
	int		have_addr;
	db_expr_t	count;
	char		*modif;
{

}

