/* Public domain. */

#ifndef _LINUX_INTERRUPT_H
#define _LINUX_INTERRUPT_H

#include <sys/task.h>

#include <machine/intr.h>
#include <linux/hardirq.h>
#include <linux/irqflags.h>
#include <linux/atomic.h>
#include <linux/compiler.h>

#define IRQF_SHARED	0

#define disable_irq(x)		intr_disable()
#define enable_irq(x)		intr_enable()

#define request_irq(irq, hdlr, flags, name, dev)	(0)
#define free_irq(irq, dev)

struct tasklet_struct {
	void (*func)(unsigned long);
	unsigned long data;
	unsigned long state;
	atomic_t count;
	struct task task;
};

#define TASKLET_STATE_SCHED	1
#define TASKLET_STATE_RUN	0

extern struct taskq *taskletq;
void tasklet_run(void *);

static inline void
tasklet_init(struct tasklet_struct *ts, void (*func)(unsigned long),
    unsigned long data)
{
	ts->func = func;
	ts->data = data;
	ts->state = 0;
	atomic_set(&ts->count, 0);
	task_set(&ts->task, tasklet_run, ts);
}

static inline int
tasklet_trylock(struct tasklet_struct *ts)
{
	return !test_and_set_bit(TASKLET_STATE_RUN, &ts->state);
}

static inline void
tasklet_unlock(struct tasklet_struct *ts)
{
	smp_mb__before_atomic();
	clear_bit(TASKLET_STATE_RUN, &ts->state);
}

static inline void
tasklet_unlock_wait(struct tasklet_struct *ts)
{
	while (test_bit(TASKLET_STATE_RUN, &ts->state))
		barrier();
}

static inline void
tasklet_kill(struct tasklet_struct *ts)
{
	clear_bit(TASKLET_STATE_SCHED, &ts->state);
	task_del(taskletq, &ts->task);
	tasklet_unlock_wait(ts);
}

static inline void
tasklet_schedule(struct tasklet_struct *ts)
{
	set_bit(TASKLET_STATE_SCHED, &ts->state);
	task_add(taskletq, &ts->task);
}

static inline void
tasklet_hi_schedule(struct tasklet_struct *ts)
{
	set_bit(TASKLET_STATE_SCHED, &ts->state);
	task_add(taskletq, &ts->task);
}

#endif
