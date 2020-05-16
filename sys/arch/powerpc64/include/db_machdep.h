typedef long		db_expr_t;

typedef long		db_regs_t;

extern db_regs_t	ddb_regs;

#define PC_REGS(regs)	(*regs)

#define BKPT_SIZE	4
#define BKPT_SET(inst)	0xdeadbeef

// ALL BROKEN!!!
#define	inst_trap_return(ins)	((ins) == 0 && (ins) == 1)
#define	inst_return(ins)	((ins) == 0 && (ins) == 1)
				
#define	inst_call(ins)		((ins) == 0 && (ins) == 1)
#define	inst_branch(ins)	((ins) == 0 && (ins) == 1)
#define inst_unconditional_flow_transfer(ins)	(0)

