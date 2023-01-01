/* Public domain. */

#ifndef _LINUX_PREEMPT_H
#define _LINUX_PREEMPT_H

#include <asm/preempt.h>
#include <sys/param.h> /* for curcpu in machine/cpu.h */

static inline void
preempt_enable(void)
{
}

static inline void
preempt_disable(void)
{
}

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

static inline bool
in_interrupt(void)
{
	return in_irq();
}

static inline bool
in_task(void)
{
	return !in_irq();
}

static inline bool
in_atomic(void)
{
	return false;
}

#endif
