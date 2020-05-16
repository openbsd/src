#ifndef _MACHINE_CPU_H_
#define _MACHINE_CPU_H_

#include <machine/intr.h>
#include <machine/frame.h>

#include <sys/device.h>
#include <sys/sched.h>

struct cpu_info {
	struct device	*ci_dev;
	struct cpu_info	*ci_next;
	struct schedstate_percpu ci_schedstate;

	struct proc	*ci_curproc;

	uint32_t	ci_ipending;
#ifdef DIAGNOSTIC
	int		ci_mutex_level;
#endif
	int		ci_want_resched;

	uint32_t	ci_randseed;
};

extern struct cpu_info cpu_info_primary;

#define curcpu()	(&cpu_info_primary)

#define MAXCPUS			1
#define CPU_IS_PRIMARY(ci)	1
#define CPU_INFO_UNIT(ci)	0
#define CPU_INFO_ITERATOR	int
#define CPU_INFO_FOREACH(cii, ci) \
	for (cii = 0, ci = curcpu(); ci != NULL; ci = NULL)


#define CLKF_INTR(frame)	0
#define CLKF_USERMODE(frame)	0
#define CLKF_PC(frame)		0

#define aston(p)		((p)->p_md.md_astpending = 1)
#define need_proftick(p)	aston(p)

#define cpu_kick(ci)
#define cpu_unidle(ci)
#define signotify(p)		setsoftast()

void need_resched(struct cpu_info *);
#define clear_resched(ci)	((ci)->ci_want_resched = 0)

void delay(u_int);
#define DELAY(x)	delay(x)

#define setsoftast()		aston(curcpu()->ci_curproc)

#define PROC_STACK(p)		0
#define PROC_PC(p)		0

#endif /* _MACHINE_CPU_H_ */
