/*	$OpenBSD: locore.h,v 1.7 2001/03/09 05:44:40 smurph Exp $	*/

#ifndef _MACHINE_LOCORE_H_
#define _MACHINE_LOCORE_H_

#include <vm/vm_param.h>

/*
 * C prototypes for various routines defined in locore_*
 */

extern unsigned int do_load_word __P((vm_offset_t address,
    boolean_t supervisor_mode));
extern unsigned int do_load_half __P((vm_offset_t address,
    boolean_t supervisor_mode));
extern unsigned int do_load_byte __P((vm_offset_t address,
    boolean_t supervisor_mode));

extern void do_store_word __P((vm_offset_t address, unsigned int data,
    boolean_t supervisor_mode));
extern void do_store_half __P((vm_offset_t address, unsigned int data,
    boolean_t supervisor_mode));
extern void do_store_byte __P((vm_offset_t address, unsigned int data,
    boolean_t supervisor_mode));

extern unsigned do_xmem_word __P((vm_offset_t address, unsigned int data,
    boolean_t supervisor_mode));
extern unsigned do_xmem_byte __P((vm_offset_t address, unsigned int data,
    boolean_t supervisor_mode));

extern unsigned read_processor_identification_register __P((void));
extern int badaddr __P((vm_offset_t addr, int size));
extern void set_cpu_number __P((unsigned number));
extern void doboot __P((void));

#if defined(MVME187) || defined(MVME188)
extern void dae_print __P((unsigned *eframe));
extern void data_access_emulation __P((unsigned *eframe));
extern int guarded_access( );
#endif 

#endif /* _MACHINE_LOCORE_H_ */
