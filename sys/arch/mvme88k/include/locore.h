/*	$OpenBSD: locore.h,v 1.22 2004/04/16 23:35:52 miod Exp $	*/

#ifndef _MACHINE_LOCORE_H_
#define _MACHINE_LOCORE_H_

#include <uvm/uvm_param.h>

/*
 * C prototypes for various routines defined in locore_* and friends
 */

/* locore_asm_routines.S */

unsigned int do_load_word(vaddr_t address,
    boolean_t supervisor_mode);
unsigned int do_load_half(vaddr_t address,
    boolean_t supervisor_mode);
unsigned int do_load_byte(vaddr_t address,
    boolean_t supervisor_mode);

void do_store_word(vaddr_t address, unsigned int data,
    boolean_t supervisor_mode);
void do_store_half(vaddr_t address, unsigned int data,
    boolean_t supervisor_mode);
void do_store_byte(vaddr_t address, unsigned int data,
    boolean_t supervisor_mode);

unsigned do_xmem_word(vaddr_t address, unsigned int data,
    boolean_t supervisor_mode);
unsigned do_xmem_byte(vaddr_t address, unsigned int data,
    boolean_t supervisor_mode);

unsigned read_processor_identification_register(void);
int badaddr(vaddr_t addr, int size);
#define badwordaddr(x) badaddr(x, 4)
void set_cpu_number(unsigned number);
void doboot(void);

int guarded_access(unsigned char *volatile address,
    unsigned len, u_char *vec);

/* locore_c_routines.c */

void dae_print(unsigned *eframe);
void data_access_emulation(unsigned *eframe);

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
void MY_info(struct trapframe *f, caddr_t p, int flags, char *s);
void MY_info_done(struct trapframe *f, int flags);
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
