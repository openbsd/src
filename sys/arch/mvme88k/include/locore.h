/*	$OpenBSD: locore.h,v 1.6 2001/03/08 00:03:22 miod Exp $	*/

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

#endif /* _MACHINE_LOCORE_H_ */
