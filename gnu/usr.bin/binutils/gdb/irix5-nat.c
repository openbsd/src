/* Native support for the SGI Iris running IRIX version 5, for GDB.

   Copyright 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996,
   1998, 1999, 2000, 2001, 2002, 2004 Free Software Foundation, Inc.

   Contributed by Alessandro Forin(af@cs.cmu.edu) at CMU
   and by Per Bothner(bothner@cs.wisc.edu) at U.Wisconsin.
   Implemented for Irix 4.x by Garrett A. Wollman.
   Modified for Irix 5.x by Ian Lance Taylor.

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
#include "inferior.h"
#include "gdbcore.h"
#include "target.h"
#include "regcache.h"

#include "gdb_string.h"
#include <sys/time.h>
#include <sys/procfs.h>
#include <setjmp.h>		/* For JB_XXX.  */

/* Prototypes for supply_gregset etc. */
#include "gregset.h"
#include "mips-tdep.h"

static void fetch_core_registers (char *, unsigned int, int, CORE_ADDR);

/* Size of elements in jmpbuf */

#define JB_ELEMENT_SIZE 4

/*
 * See the comment in m68k-tdep.c regarding the utility of these functions.
 *
 * These definitions are from the MIPS SVR4 ABI, so they may work for
 * any MIPS SVR4 target.
 */

void
supply_gregset (gregset_t *gregsetp)
{
  int regi;
  greg_t *regp = &(*gregsetp)[0];
  int gregoff = sizeof (greg_t) - mips_isa_regsize (current_gdbarch);
  static char zerobuf[32] = {0};

  for (regi = 0; regi <= CTX_RA; regi++)
    regcache_raw_supply (current_regcache, regi,
			 (char *) (regp + regi) + gregoff);

  regcache_raw_supply (current_regcache, mips_regnum (current_gdbarch)->pc,
		       (char *) (regp + CTX_EPC) + gregoff);
  regcache_raw_supply (current_regcache, mips_regnum (current_gdbarch)->hi,
		       (char *) (regp + CTX_MDHI) + gregoff);
  regcache_raw_supply (current_regcache, mips_regnum (current_gdbarch)->lo,
		       (char *) (regp + CTX_MDLO) + gregoff);
  regcache_raw_supply (current_regcache, mips_regnum (current_gdbarch)->cause,
		       (char *) (regp + CTX_CAUSE) + gregoff);

  /* Fill inaccessible registers with zero.  */
  regcache_raw_supply (current_regcache, mips_regnum (current_gdbarch)->badvaddr, zerobuf);
}

void
fill_gregset (gregset_t *gregsetp, int regno)
{
  int regi;
  greg_t *regp = &(*gregsetp)[0];

  /* Under Irix6, if GDB is built with N32 ABI and is debugging an O32
     executable, we have to sign extend the registers to 64 bits before
     filling in the gregset structure.  */

  for (regi = 0; regi <= CTX_RA; regi++)
    if ((regno == -1) || (regno == regi))
      *(regp + regi) =
	extract_signed_integer (&deprecated_registers[DEPRECATED_REGISTER_BYTE (regi)],
				register_size (current_gdbarch, regi));

  if ((regno == -1) || (regno == PC_REGNUM))
    *(regp + CTX_EPC) =
      extract_signed_integer (&deprecated_registers[DEPRECATED_REGISTER_BYTE (mips_regnum (current_gdbarch)->pc)],
			      register_size (current_gdbarch, mips_regnum (current_gdbarch)->pc));

  if ((regno == -1) || (regno == mips_regnum (current_gdbarch)->cause))
    *(regp + CTX_CAUSE) =
      extract_signed_integer (&deprecated_registers[DEPRECATED_REGISTER_BYTE (mips_regnum (current_gdbarch)->cause)],
			      register_size (current_gdbarch, mips_regnum (current_gdbarch)->cause));

  if ((regno == -1)
      || (regno == mips_regnum (current_gdbarch)->hi))
    *(regp + CTX_MDHI) =
      extract_signed_integer (&deprecated_registers[DEPRECATED_REGISTER_BYTE (mips_regnum (current_gdbarch)->hi)],
			      register_size (current_gdbarch, mips_regnum (current_gdbarch)->hi));

  if ((regno == -1) || (regno == mips_regnum (current_gdbarch)->lo))
    *(regp + CTX_MDLO) =
      extract_signed_integer (&deprecated_registers[DEPRECATED_REGISTER_BYTE (mips_regnum (current_gdbarch)->lo)],
			      register_size (current_gdbarch, mips_regnum (current_gdbarch)->lo));
}

/*
 * Now we do the same thing for floating-point registers.
 * We don't bother to condition on FP0_REGNUM since any
 * reasonable MIPS configuration has an R3010 in it.
 *
 * Again, see the comments in m68k-tdep.c.
 */

void
supply_fpregset (fpregset_t *fpregsetp)
{
  int regi;
  static char zerobuf[32] = {0};

  /* FIXME, this is wrong for the N32 ABI which has 64 bit FP regs. */

  for (regi = 0; regi < 32; regi++)
    regcache_raw_supply (current_regcache, FP0_REGNUM + regi,
			 (char *) &fpregsetp->fp_r.fp_regs[regi]);

  regcache_raw_supply (current_regcache,
		       mips_regnum (current_gdbarch)->fp_control_status,
		       (char *) &fpregsetp->fp_csr);

  /* FIXME: how can we supply FCRIR?  SGI doesn't tell us. */
  regcache_raw_supply (current_regcache,
		       mips_regnum (current_gdbarch)->fp_implementation_revision,
		       zerobuf);
}

void
fill_fpregset (fpregset_t *fpregsetp, int regno)
{
  int regi;
  char *from, *to;

  /* FIXME, this is wrong for the N32 ABI which has 64 bit FP regs. */

  for (regi = FP0_REGNUM; regi < FP0_REGNUM + 32; regi++)
    {
      if ((regno == -1) || (regno == regi))
	{
	  from = (char *) &deprecated_registers[DEPRECATED_REGISTER_BYTE (regi)];
	  to = (char *) &(fpregsetp->fp_r.fp_regs[regi - FP0_REGNUM]);
	  memcpy (to, from, register_size (current_gdbarch, regi));
	}
    }

  if ((regno == -1)
      || (regno == mips_regnum (current_gdbarch)->fp_control_status))
    fpregsetp->fp_csr = *(unsigned *) &deprecated_registers[DEPRECATED_REGISTER_BYTE (mips_regnum (current_gdbarch)->fp_control_status)];
}


/* Figure out where the longjmp will land.
   We expect the first arg to be a pointer to the jmp_buf structure from which
   we extract the pc (JB_PC) that we will land at.  The pc is copied into PC.
   This routine returns true on success. */

int
get_longjmp_target (CORE_ADDR *pc)
{
  char *buf;
  CORE_ADDR jb_addr;

  buf = alloca (TARGET_PTR_BIT / TARGET_CHAR_BIT);
  jb_addr = read_register (A0_REGNUM);

  if (target_read_memory (jb_addr + JB_PC * JB_ELEMENT_SIZE, buf,
			  TARGET_PTR_BIT / TARGET_CHAR_BIT))
    return 0;

  *pc = extract_unsigned_integer (buf, TARGET_PTR_BIT / TARGET_CHAR_BIT);

  return 1;
}

/* Provide registers to GDB from a core file.

   CORE_REG_SECT points to an array of bytes, which were obtained from
   a core file which BFD thinks might contain register contents. 
   CORE_REG_SIZE is its size.

   Normally, WHICH says which register set corelow suspects this is:
     0 --- the general-purpose register set
     2 --- the floating-point register set
   However, for Irix 5, WHICH isn't used.

   REG_ADDR is also unused.  */

static void
fetch_core_registers (char *core_reg_sect, unsigned core_reg_size,
		      int which, CORE_ADDR reg_addr)
{
  if (core_reg_size == deprecated_register_bytes ())
    {
      memcpy ((char *) deprecated_registers, core_reg_sect, core_reg_size);
    }
  else if (mips_isa_regsize (current_gdbarch) == 4 &&
	   core_reg_size == (2 * mips_isa_regsize (current_gdbarch)) * NUM_REGS)
    {
      /* This is a core file from a N32 executable, 64 bits are saved
         for all registers.  */
      char *srcp = core_reg_sect;
      char *dstp = deprecated_registers;
      int regno;

      for (regno = 0; regno < NUM_REGS; regno++)
	{
	  if (regno >= FP0_REGNUM && regno < (FP0_REGNUM + 32))
	    {
	      /* FIXME, this is wrong, N32 has 64 bit FP regs, but GDB
	         currently assumes that they are 32 bit.  */
	      *dstp++ = *srcp++;
	      *dstp++ = *srcp++;
	      *dstp++ = *srcp++;
	      *dstp++ = *srcp++;
	      if (register_size (current_gdbarch, regno) == 4)
		{
		  /* copying 4 bytes from eight bytes?
		     I don't see how this can be right...  */
		  srcp += 4;
		}
	      else
		{
		  /* copy all 8 bytes (sizeof(double)) */
		  *dstp++ = *srcp++;
		  *dstp++ = *srcp++;
		  *dstp++ = *srcp++;
		  *dstp++ = *srcp++;
		}
	    }
	  else
	    {
	      srcp += 4;
	      *dstp++ = *srcp++;
	      *dstp++ = *srcp++;
	      *dstp++ = *srcp++;
	      *dstp++ = *srcp++;
	    }
	}
    }
  else
    {
      warning ("wrong size gregset struct in core file");
      return;
    }

  deprecated_registers_fetched ();
}

/* Register that we are able to handle irix5 core file formats.
   This really is bfd_target_unknown_flavour */

static struct core_fns irix5_core_fns =
{
  bfd_target_unknown_flavour,		/* core_flavour */
  default_check_format,			/* check_format */
  default_core_sniffer,			/* core_sniffer */
  fetch_core_registers,			/* core_read_registers */
  NULL					/* next */
};

void
_initialize_core_irix5 (void)
{
  deprecated_add_core_fns (&irix5_core_fns);
}
