/* Public domain. */

#ifndef _LINUX_PREEMPT_H
#define _LINUX_PREEMPT_H

#include <asm/preempt.h>

#define preempt_enable()
#define preempt_disable()

static inline bool
in_irq(void)
{
#if defined(__amd64__) || defined(__arm__) || defined(__arm64__) || \
    defined(__i386__) || defined(__powerpc64__) || defined(__riscv64__)
	return (curcpu()->ci_idepth > 0);
#else
	return false;
#endif
}

#define in_interrupt()	in_irq()
#define in_task()	(!in_irq())
#define in_atomic()	0

#endif
