/*	$OpenBSD: locore.h,v 1.28 2005/09/13 19:12:21 miod Exp $	*/

#ifndef _MACHINE_LOCORE_H_
#define _MACHINE_LOCORE_H_

/*
 * C prototypes for various routines defined in locore_* and friends
 */

/* subr.S */

unsigned read_processor_identification_register(void);
int badaddr(vaddr_t addr, int size);
#define badwordaddr(x) badaddr(x, 4)
void set_cpu_number(unsigned number);
void doboot(void);

/* locore_c_routines.c */

unsigned getipl(void);

/* machdep.c */

int intr_findvec(int, int, int);
void myetheraddr(u_char *cp);

extern volatile u_int8_t *ivec[];

/* eh.S */

void sigsys(void);
void sigtrap(void);
void stepbpt(void);
void userbpt(void);
void syscall_handler(void);
void cache_flush_handler(void);
void m88110_sigsys(void);
void m88110_sigtrap(void);
void m88110_stepbpt(void);
void m88110_userbpt(void);
void m88110_syscall_handler(void);
void m88110_cache_flush_handler(void);

#endif /* _MACHINE_LOCORE_H_ */
