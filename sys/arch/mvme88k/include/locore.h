/*	$OpenBSD: locore.h,v 1.13 2001/12/16 23:49:46 miod Exp $	*/

#ifndef _MACHINE_LOCORE_H_
#define _MACHINE_LOCORE_H_

#include <uvm/uvm_param.h>

/*
 * C prototypes for various routines defined in locore_* and friends
 */

/* locore_asm_routines.S */

unsigned int do_load_word __P((vm_offset_t address,
    boolean_t supervisor_mode));
unsigned int do_load_half __P((vm_offset_t address,
    boolean_t supervisor_mode));
unsigned int do_load_byte __P((vm_offset_t address,
    boolean_t supervisor_mode));

void do_store_word __P((vm_offset_t address, unsigned int data,
    boolean_t supervisor_mode));
void do_store_half __P((vm_offset_t address, unsigned int data,
    boolean_t supervisor_mode));
void do_store_byte __P((vm_offset_t address, unsigned int data,
    boolean_t supervisor_mode));

unsigned do_xmem_word __P((vm_offset_t address, unsigned int data,
    boolean_t supervisor_mode));
unsigned do_xmem_byte __P((vm_offset_t address, unsigned int data,
    boolean_t supervisor_mode));

unsigned read_processor_identification_register __P((void));
int badaddr __P((vm_offset_t addr, int size));
#define badwordaddr(x) badaddr(x, 4)
void set_cpu_number __P((unsigned number));
void doboot __P((void));
int db_are_interrupts_disabled __P((void));

void fubail __P((void));
void subail __P((void));

#if defined(MVME187) || defined(MVME188)
int guarded_access __P((volatile unsigned char *address,
    unsigned len, u_char *vec));
#endif 


/* locore_c_routines.c */

#if defined(MVME187) || defined(MVME188)
void dae_print __P((unsigned *eframe));
void data_access_emulation __P((unsigned *eframe));
#endif 
#ifdef MVME188
unsigned int safe_level __P((unsigned mask, unsigned curlevel));
#if 0
void block_obio_interrupt __P((unsigned mask));
void unblock_obio_interrupt __P((unsigned mask));
#endif
#endif 
unsigned spl __P((void));
unsigned getipl __P((void));
#ifdef DDB
unsigned db_spl __P((void));
unsigned db_getipl __P((void));
#endif


/* machdep.c */

void _doboot __P((void));
vm_offset_t get_slave_stack __P((void));
void slave_pre_main __P((void));
int slave_main __P((void));
int intr_findvec __P((int start, int end));
void bugsyscall __P((void));
void myetheraddr __P((u_char *cp));
void dosoftint __P((void));
void MY_info __P((struct trapframe *f, caddr_t p, int flags, char *s));
void MY_info_done __P((struct trapframe *f, int flags));
void mvme_bootstrap __P((void));
#ifdef MVME188
void m188_reset __P((void));
void m188_ext_int __P((u_int v, struct m88100_saved_state *eframe));
#endif
#if defined(MVME187) || defined(MVME197)
void sbc_ext_int __P((u_int v, struct m88100_saved_state *eframe));
#endif


/* eh.S */

struct proc;
void proc_do_uret __P((struct proc *));
#if defined(MVME187) || defined(MVME188)
void sigsys __P((void));
void sigtrap __P((void));
void stepbpt __P((void));
void userbpt __P((void));
void syscall_handler __P((void));
#endif 
#if defined(MVME197)
void m197_sigsys __P((void));
void m197_sigtrap __P((void));
void m197_stepbpt __P((void));
void m197_userbpt __P((void));
void m197_syscall_handler __P((void));
#endif 

/* process.S */
void savectx __P((struct pcb *));
void switch_exit __P((struct proc *));

#endif /* _MACHINE_LOCORE_H_ */
