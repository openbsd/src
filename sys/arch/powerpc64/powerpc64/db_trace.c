#include <sys/param.h>

#include <machine/db_machdep.h>

#include <ddb/db_interface.h>
#include <ddb/db_variables.h>

db_regs_t ddb_regs;

struct db_variable db_regs[] = {
};

struct db_variable *db_eregs = db_regs + nitems(db_regs);

void
db_stack_trace_print(db_expr_t addr, int have_addr, db_expr_t count,
    char *modif, int (*pr)(const char *, ...))
{
}
