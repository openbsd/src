/*	$OpenBSD: locore.h,v 1.24 2004/08/01 17:18:05 miod Exp $	*/

#ifndef _MACHINE_LOCORE_H_
#define _MACHINE_LOCORE_H_

#include <uvm/uvm_param.h>

/*
 * C prototypes for various routines defined in locore_* and friends
 */

/* subr.S */

unsigned read_processor_identification_register(void);
int badaddr(vaddr_t addr, int size);
#define badwordaddr(x) badaddr(x, 4)
void set_cpu_number(unsigned number);
void doboot(void);

int guarded_access(unsigned char *volatile address,
    unsigned len, u_char *vec);

/* locore_c_routines.c */

unsigned getipl(void);

/* machdep.c */

void _doboot(void);
vaddr_t get_slave_stack(void);
void slave_pre_main(void);
int slave_main(void);
int intr_findvec(int, int, int);
void bugsyscall(void);
void myetheraddr(u_char *cp);
void dosoftint(void);
void mvme_bootstrap(void);
void m187_ext_int(u_int v, struct trapframe *eframe);
void m188_reset(void);
void m188_ext_int(u_int v, struct trapframe *eframe);
unsigned int safe_level(unsigned mask, unsigned curlevel);
void m197_ext_int(u_int v, struct trapframe *eframe);

/* eh.S */

struct proc;
void proc_do_uret(struct proc *);
void sigsys(void);
void sigtrap(void);
void stepbpt(void);
void userbpt(void);
void syscall_handler(void);
void m88110_sigsys(void);
void m88110_sigtrap(void);
void m88110_stepbpt(void);
void m88110_userbpt(void);
void m88110_syscall_handler(void);

/* process.S */
void savectx(struct pcb *);
void switch_exit(struct proc *);

#endif /* _MACHINE_LOCORE_H_ */
