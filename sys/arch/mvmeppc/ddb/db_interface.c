/*	$OpenBSD: db_interface.c,v 1.2 2002/03/14 01:26:41 millert Exp $	*/

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>

#include <machine/db_machdep.h>
#include <machine/frame.h>

#include <ddb/db_sym.h>
#include <ddb/db_command.h>
#include <ddb/db_extern.h>
#include <ddb/db_access.h>
#include <ddb/db_output.h>

extern label_t *db_recover;

void ddb_trap(void);                   /* Call into trap_subr.S */
int ddb_trap_glue(struct trapframe *); /* Called from trap_subr.S */

void
Debugger()
{
#ifdef DDB
	ddb_trap();
#else
	mvmeprom_retunr();
#endif 
}

int
ddb_trap_glue(frame)
        struct trapframe *frame;
{
        if (!(frame->srr1 & PSL_PR)
            && (frame->exc == EXC_TRC
                || (frame->exc == EXC_PGM
                    && (frame->srr1 & 0x20000))
                || frame->exc == EXC_BPT)) {

                bcopy(frame->fixreg, DDB_REGS->r, 32 * sizeof(u_int32_t));
                DDB_REGS->iar = frame->srr0;
                DDB_REGS->msr = frame->srr1;

                db_trap(T_BREAKPOINT, 0);

                bcopy(DDB_REGS->r, frame->fixreg, 32 * sizeof(u_int32_t));

                return 1;
        }
        return 0;
}

struct db_command db_machine_cmds[] =
{
    {(char  *) 0,}
};

void
kdb_init()
{
#ifdef DB_MACHINE_COMMANDS
	db_machine_commands_install(db_machine_cmds);
#endif
	ddb_init();

	db_printf("ddb enabled\n");
}

int
kdb_trap(type, v)
        int type;
        void *v;
{
        struct trapframe *frame = v;

        switch (type) {
        case T_BREAKPOINT:
        case -1:
                break;
        default:
                if (db_recover != 0) {
                        db_error("Faulted in DDB; continuing...\n");
                        /*NOTREACHED*/
                }
        }

        /* XXX Should switch to kdb's own stack here. */

        bcopy(frame->fixreg, DDB_REGS->r, 32 * sizeof(u_int32_t));
        DDB_REGS->iar = frame->srr0;
        DDB_REGS->msr = frame->srr1;

        db_trap(T_BREAKPOINT, 0);

        bcopy(DDB_REGS->r, frame->fixreg, 32 * sizeof(u_int32_t));
        frame->srr0 = DDB_REGS->iar;
        frame->srr1 = DDB_REGS->msr;

        return 1;
}

