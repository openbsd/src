/* Native-dependent code for NetBSD/i386.

   Copyright 2004 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "gdbcore.h"
#include "regcache.h"
#include "target.h"

#include "i386-tdep.h"
#include "i386bsd-nat.h"

/* Support for debugging kernel virtual memory images.  */

#include <sys/types.h>
#include <machine/frame.h>
#include <machine/pcb.h>

#include "bsd-kvm.h"

static int
i386nbsd_supply_pcb (struct regcache *regcache, struct pcb *pcb)
{
  struct switchframe sf;

  /* The following is true for NetBSD 1.6.2:

     The pcb contains %esp and %ebp at the point of the context switch
     in cpu_switch().  At that point we have a stack frame as
     described by `struct switchframe', which for NetBSD 1.6.2 has the
     following layout:

     interrupt level
     %edi
     %esi
     %ebx
     %eip

     we reconstruct the register state as it would look when we just
     returned from cpu_switch().  */

  /* The stack pointer shouldn't be zero.  */
  if (pcb->pcb_esp == 0)
    return 0;

  read_memory (pcb->pcb_esp, (char *) &sf, sizeof sf);
  pcb->pcb_esp += sizeof (struct switchframe);
  regcache_raw_supply (regcache, I386_EDI_REGNUM, &sf.sf_edi);
  regcache_raw_supply (regcache, I386_ESI_REGNUM, &sf.sf_esi);
  regcache_raw_supply (regcache, I386_EBP_REGNUM, &pcb->pcb_ebp);
  regcache_raw_supply (regcache, I386_ESP_REGNUM, &pcb->pcb_esp);
  regcache_raw_supply (regcache, I386_EBX_REGNUM, &sf.sf_ebx);
  regcache_raw_supply (regcache, I386_EIP_REGNUM, &sf.sf_eip);

  return 1;
}


/* Provide a prototype to silence -Wmissing-prototypes.  */
void _initialize_i386nbsd_nat (void);

void
_initialize_i386nbsd_nat (void)
{
  /* We've got nothing to add to the common *BSD/i386 target.  */
  add_target (i386bsd_target ());

  /* Support debugging kernel virtual memory images.  */
  bsd_kvm_add_target (i386nbsd_supply_pcb);
}
