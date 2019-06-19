/* Public domain. */

#ifndef _LINUX_STOP_MACHINE_H
#define _LINUX_STOP_MACHINE_H

#include <machine/intr.h>

typedef int (*cpu_stop_fn_t)(void *arg);

static inline int
stop_machine(cpu_stop_fn_t fn, void *arg, void *cpus)
{
	int r;
	intr_disable();
	r = (*fn)(arg);
	intr_enable();
	return r;
}

#endif
