/*	$OpenBSD: isa_machdep.h,v 1.8 2003/10/31 03:55:06 drahn Exp $ */

/*
 * Copyright (c) 1997 Per Fogelstrom
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed under OpenBSD by
 *	Per Fogelstrom, Opsycon AB, Sweden.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#ifndef _ISA_MACHDEP_H_
#define _ISA_MACHDEP_H_

typedef struct ppc_isa_bus *isa_chipset_tag_t;

/*
 *      I/O macros to access isa bus ports/memory.
 *      At the first glance theese macros may seem inefficient.
 *      However, the cpu executes an instruction every <10 ns
 *      so the bus is much slower so it doesn't matter, really.
 */
extern bus_space_handle_t ppc_isa_io_vaddr;
#define isa_outb(x,y)   outb(ppc_isa_io_vaddr + (x), (y))
#define isa_inb(x)   inb(ppc_isa_io_vaddr + (x))
 
struct ppc_isa_bus {
        void    *ic_data;

        void    (*ic_attach_hook) (struct device *, struct device *,
                    struct isabus_attach_args *);
        void    *(*ic_intr_establish) (void *, int, int, int,
                    int (*)(void *), void *, char *);
        void    (*ic_intr_disestablish) (void *, void *);
};


/*
 * Functions provided to machine-independent ISA code.
 */
#define isa_attach_hook(p, s, a)                             /*           \
    (*(a)->iba_ic->ic_attach_hook)((p), (s), (a)) */
#define isa_intr_establish(c, i, t, l, f, a, w)                         \
    (*(c)->ic_intr_establish)((c)->ic_data, (i), (t), (l), (f), (a), (w))
#define isa_intr_disestablish(c, h)                                     \
    (*(c)->ic_intr_disestablish)((c)->ic_data, (h))

#define __NO_ISA_INTR_CHECK	/* FIXME */

#define ICU_LEN	16		/* Number of possible interrupt sources */

extern void * isabr_intr_establish(void *, int, int, int,                             int (*ih_fun) (void *), void *, char *);                              
#endif /* _ISA_MACHDEP_H_ */
