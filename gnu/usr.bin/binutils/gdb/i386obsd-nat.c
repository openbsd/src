/* Native-dependent code for OpenBSD/i386.

   Copyright 2002, 2003, 2004 Free Software Foundation, Inc.

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
#include "obsd-nat.h"
#include "i386bsd-nat.h"

#include <sys/param.h>
#include <sys/sysctl.h>

/* Support for debugging kernel virtual memory images.  */

#include <machine/frame.h>
#include <machine/pcb.h>

#include "bsd-kvm.h"

static int
i386obsd_supply_pcb (struct regcache *regcache, struct pcb *pcb)
{
  struct switchframe sf;

  /* The following is true for OpenBSD 4.3:

     The pcb contains %esp and %ebp at the point of the context switch
     in cpu_switchto().  At that point we have a stack frame as
     described by `struct switchframe', which for OpenBSD 4.3 has the
     following layout:

     %edi
     %esi
     %ebx
     %eip

     we reconstruct the register state as it would look when we just
     returned from cpu_switchto().

     For crash dumps, the state is saved by savectx().  In that case
     we just reconstruct the register state as if we just returned
     from dumsys().  */

  /* The stack pointer shouldn't be zero.  */
  if (pcb->pcb_esp == 0)
    return 0;

  if ((pcb->pcb_flags & PCB_SAVECTX) == 0)
    {
      /* Yes, we have a frame that matches cpu_switchto().  */
      read_memory (pcb->pcb_esp, (char *) &sf, sizeof sf);
      pcb->pcb_esp += sizeof (struct switchframe);
      regcache_raw_supply (regcache, I386_EDI_REGNUM, &sf.sf_edi);
      regcache_raw_supply (regcache, I386_ESI_REGNUM, &sf.sf_esi);
      regcache_raw_supply (regcache, I386_EBX_REGNUM, &sf.sf_ebx);
      regcache_raw_supply (regcache, I386_EIP_REGNUM, &sf.sf_eip);
    }
  else
    {
      /* No, the pcb must have been last updated by savectx().  */
      pcb->pcb_esp = pcb->pcb_ebp;
      pcb->pcb_ebp = read_memory_integer(pcb->pcb_esp, 4);
      sf.sf_eip = read_memory_integer(pcb->pcb_esp + 4, 4);
      regcache_raw_supply (regcache, I386_EIP_REGNUM, &sf.sf_eip);
    }

  regcache_raw_supply (regcache, I386_EBP_REGNUM, &pcb->pcb_ebp);
  regcache_raw_supply (regcache, I386_ESP_REGNUM, &pcb->pcb_esp);

  return 1;
}


/* Prevent warning from -Wmissing-prototypes.  */
void _initialize_i386obsd_nat (void);

void
_initialize_i386obsd_nat (void)
{
  struct target_ops *t;

  /* OpenBSD provides a vm.psstrings sysctl that we can use to locate
     the sigtramp.  That way we can still recognize a sigtramp if its
     location is changed in a new kernel.  This is especially
     important for OpenBSD, since it uses a different memory layout
     than NetBSD, yet we cannot distinguish between the two.

     Of course this is still based on the assumption that the sigtramp
     is placed directly under the location where the program arguments
     and environment can be found.  */
#ifdef VM_PSSTRINGS
  {
    struct _ps_strings _ps;
    int mib[2];
    size_t len;

    mib[0] = CTL_VM;
    mib[1] = VM_PSSTRINGS;
    len = sizeof (_ps);
    if (sysctl (mib, 2, &_ps, &len, NULL, 0) == 0)
      {
	i386obsd_sigtramp_start_addr = (CORE_ADDR)_ps.val - 128;
	i386obsd_sigtramp_end_addr = (CORE_ADDR)_ps.val;
      }
  }
#endif

  /* Add some extra features to the common *BSD/i386 target.  */
  t = i386bsd_target ();
  t->to_pid_to_str = obsd_pid_to_str;
  t->to_find_new_threads = obsd_find_new_threads;
  t->to_wait = obsd_wait;
  add_target (t);

  /* Support debugging kernel virtual memory images.  */
  bsd_kvm_add_target (i386obsd_supply_pcb);
}
