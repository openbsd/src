#include "config.h"
#include "defines.h"
#include "compat.h"
#include "make.h"

struct engine {
	void (*run_list)(Lst, bool *, bool *);
	void (*node_updated)(GNode *);
	void (*init)(void);
} 
	compat_engine = { Compat_Run, Compat_Update, Compat_Init }, 
	parallel_engine = { Make_Run, Make_Update, Make_Init }, 
	*engine;

void
choose_engine(bool compat)
{
	engine = compat ? &compat_engine: &parallel_engine;
	engine->init();
}

void
engine_run_list(Lst l, bool *has_errors, bool *out_of_date)
{
	engine->run_list(l, has_errors, out_of_date);
}

void
engine_node_updated(GNode *gn)
{
	engine->node_updated(gn);
}
