/* PPC GNU/Linux native support.

   Copyright 1988, 1989, 1991, 1992, 1994, 1996, 2000, 2001, 2002,
   2003 Free Software Foundation, Inc.

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
#include "gdb_string.h"
#include "frame.h"
#include "inferior.h"
#include "gdbcore.h"
#include "regcache.h"
#include "gdb_assert.h"

#include <sys/types.h>
#include <sys/param.h>
#include <signal.h>
#include <sys/user.h>
#include <sys/ioctl.h>
#include "gdb_wait.h"
#include <fcntl.h>
#include <sys/procfs.h>
#include <sys/ptrace.h>

/* Prototypes for supply_gregset etc. */
#include "gregset.h"
#include "ppc-tdep.h"

#ifndef PT_READ_U
#define PT_READ_U PTRACE_PEEKUSR
#endif
#ifndef PT_WRITE_U
#define PT_WRITE_U PTRACE_POKEUSR
#endif

/* Default the type of the ptrace transfer to int.  */
#ifndef PTRACE_XFER_TYPE
#define PTRACE_XFER_TYPE int
#endif

/* Glibc's headers don't define PTRACE_GETVRREGS so we cannot use a
   configure time check.  Some older glibc's (for instance 2.2.1)
   don't have a specific powerpc version of ptrace.h, and fall back on
   a generic one.  In such cases, sys/ptrace.h defines
   PTRACE_GETFPXREGS and PTRACE_SETFPXREGS to the same numbers that
   ppc kernel's asm/ptrace.h defines PTRACE_GETVRREGS and
   PTRACE_SETVRREGS to be.  This also makes a configury check pretty
   much useless.  */

/* These definitions should really come from the glibc header files,
   but Glibc doesn't know about the vrregs yet.  */
#ifndef PTRACE_GETVRREGS
#define PTRACE_GETVRREGS 18
#define PTRACE_SETVRREGS 19
#endif


/* Similarly for the ptrace requests for getting / setting the SPE
   registers (ev0 -- ev31, acc, and spefscr).  See the description of
   gdb_evrregset_t for details.  */
#ifndef PTRACE_GETEVRREGS
#define PTRACE_GETEVRREGS 20
#define PTRACE_SETEVRREGS 21
#endif


/* This oddity is because the Linux kernel defines elf_vrregset_t as
   an array of 33 16 bytes long elements.  I.e. it leaves out vrsave.
   However the PTRACE_GETVRREGS and PTRACE_SETVRREGS requests return
   the vrsave as an extra 4 bytes at the end.  I opted for creating a
   flat array of chars, so that it is easier to manipulate for gdb.

   There are 32 vector registers 16 bytes longs, plus a VSCR register
   which is only 4 bytes long, but is fetched as a 16 bytes
   quantity. Up to here we have the elf_vrregset_t structure.
   Appended to this there is space for the VRSAVE register: 4 bytes.
   Even though this vrsave register is not included in the regset
   typedef, it is handled by the ptrace requests.

   Note that GNU/Linux doesn't support little endian PPC hardware,
   therefore the offset at which the real value of the VSCR register
   is located will be always 12 bytes.

   The layout is like this (where x is the actual value of the vscr reg): */

/* *INDENT-OFF* */
/*
   |.|.|.|.|.....|.|.|.|.||.|.|.|x||.|
   <------->     <-------><-------><->
     VR0           VR31     VSCR    VRSAVE
*/
/* *INDENT-ON* */

#define SIZEOF_VRREGS 33*16+4

typedef char gdb_vrregset_t[SIZEOF_VRREGS];


/* On PPC processors that support the the Signal Processing Extension
   (SPE) APU, the general-purpose registers are 64 bits long.
   However, the ordinary Linux kernel PTRACE_PEEKUSR / PTRACE_POKEUSR
   / PT_READ_U / PT_WRITE_U ptrace calls only access the lower half of
   each register, to allow them to behave the same way they do on
   non-SPE systems.  There's a separate pair of calls,
   PTRACE_GETEVRREGS / PTRACE_SETEVRREGS, that read and write the top
   halves of all the general-purpose registers at once, along with
   some SPE-specific registers.

   GDB itself continues to claim the general-purpose registers are 32
   bits long.  It has unnamed raw registers that hold the upper halves
   of the gprs, and the the full 64-bit SIMD views of the registers,
   'ev0' -- 'ev31', are pseudo-registers that splice the top and
   bottom halves together.

   This is the structure filled in by PTRACE_GETEVRREGS and written to
   the inferior's registers by PTRACE_SETEVRREGS.  */
struct gdb_evrregset_t
{
  unsigned long evr[32];
  unsigned long long acc;
  unsigned long spefscr;
};


/* Non-zero if our kernel may support the PTRACE_GETVRREGS and
   PTRACE_SETVRREGS requests, for reading and writing the Altivec
   registers.  Zero if we've tried one of them and gotten an
   error.  */
int have_ptrace_getvrregs = 1;


/* Non-zero if our kernel may support the PTRACE_GETEVRREGS and
   PTRACE_SETEVRREGS requests, for reading and writing the SPE
   registers.  Zero if we've tried one of them and gotten an
   error.  */
int have_ptrace_getsetevrregs = 1;


int
kernel_u_size (void)
{
  return (sizeof (struct user));
}

/* *INDENT-OFF* */
/* registers layout, as presented by the ptrace interface:
PT_R0, PT_R1, PT_R2, PT_R3, PT_R4, PT_R5, PT_R6, PT_R7,
PT_R8, PT_R9, PT_R10, PT_R11, PT_R12, PT_R13, PT_R14, PT_R15,
PT_R16, PT_R17, PT_R18, PT_R19, PT_R20, PT_R21, PT_R22, PT_R23,
PT_R24, PT_R25, PT_R26, PT_R27, PT_R28, PT_R29, PT_R30, PT_R31,
PT_FPR0, PT_FPR0 + 2, PT_FPR0 + 4, PT_FPR0 + 6, PT_FPR0 + 8, PT_FPR0 + 10, PT_FPR0 + 12, PT_FPR0 + 14,
PT_FPR0 + 16, PT_FPR0 + 18, PT_FPR0 + 20, PT_FPR0 + 22, PT_FPR0 + 24, PT_FPR0 + 26, PT_FPR0 + 28, PT_FPR0 + 30,
PT_FPR0 + 32, PT_FPR0 + 34, PT_FPR0 + 36, PT_FPR0 + 38, PT_FPR0 + 40, PT_FPR0 + 42, PT_FPR0 + 44, PT_FPR0 + 46,
PT_FPR0 + 48, PT_FPR0 + 50, PT_FPR0 + 52, PT_FPR0 + 54, PT_FPR0 + 56, PT_FPR0 + 58, PT_FPR0 + 60, PT_FPR0 + 62,
PT_NIP, PT_MSR, PT_CCR, PT_LNK, PT_CTR, PT_XER, PT_MQ */
/* *INDENT_ON * */

static int
ppc_register_u_addr (int regno)
{
  int u_addr = -1;
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  /* NOTE: cagney/2003-11-25: This is the word size used by the ptrace
     interface, and not the wordsize of the program's ABI.  */
  int wordsize = sizeof (PTRACE_XFER_TYPE);

  /* General purpose registers occupy 1 slot each in the buffer */
  if (regno >= tdep->ppc_gp0_regnum 
      && regno < tdep->ppc_gp0_regnum + ppc_num_gprs)
    u_addr = ((regno - tdep->ppc_gp0_regnum + PT_R0) * wordsize);

  /* Floating point regs: eight bytes each in both 32- and 64-bit
     ptrace interfaces.  Thus, two slots each in 32-bit interface, one
     slot each in 64-bit interface.  */
  if (tdep->ppc_fp0_regnum >= 0
      && regno >= tdep->ppc_fp0_regnum
      && regno < tdep->ppc_fp0_regnum + ppc_num_fprs)
    u_addr = (PT_FPR0 * wordsize) + ((regno - tdep->ppc_fp0_regnum) * 8);

  /* UISA special purpose registers: 1 slot each */
  if (regno == PC_REGNUM)
    u_addr = PT_NIP * wordsize;
  if (regno == tdep->ppc_lr_regnum)
    u_addr = PT_LNK * wordsize;
  if (regno == tdep->ppc_cr_regnum)
    u_addr = PT_CCR * wordsize;
  if (regno == tdep->ppc_xer_regnum)
    u_addr = PT_XER * wordsize;
  if (regno == tdep->ppc_ctr_regnum)
    u_addr = PT_CTR * wordsize;
#ifdef PT_MQ
  if (regno == tdep->ppc_mq_regnum)
    u_addr = PT_MQ * wordsize;
#endif
  if (regno == tdep->ppc_ps_regnum)
    u_addr = PT_MSR * wordsize;
  if (tdep->ppc_fpscr_regnum >= 0
      && regno == tdep->ppc_fpscr_regnum)
    u_addr = PT_FPSCR * wordsize;

  return u_addr;
}

/* The Linux kernel ptrace interface for AltiVec registers uses the
   registers set mechanism, as opposed to the interface for all the
   other registers, that stores/fetches each register individually.  */
static void
fetch_altivec_register (int tid, int regno)
{
  int ret;
  int offset = 0;
  gdb_vrregset_t regs;
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  int vrregsize = register_size (current_gdbarch, tdep->ppc_vr0_regnum);

  ret = ptrace (PTRACE_GETVRREGS, tid, 0, &regs);
  if (ret < 0)
    {
      if (errno == EIO)
        {
          have_ptrace_getvrregs = 0;
          return;
        }
      perror_with_name ("Unable to fetch AltiVec register");
    }
 
  /* VSCR is fetched as a 16 bytes quantity, but it is really 4 bytes
     long on the hardware.  We deal only with the lower 4 bytes of the
     vector.  VRSAVE is at the end of the array in a 4 bytes slot, so
     there is no need to define an offset for it.  */
  if (regno == (tdep->ppc_vrsave_regnum - 1))
    offset = vrregsize - register_size (current_gdbarch, tdep->ppc_vrsave_regnum);
  
  regcache_raw_supply (current_regcache, regno,
		       regs + (regno - tdep->ppc_vr0_regnum) * vrregsize + offset);
}

/* Fetch the top 32 bits of TID's general-purpose registers and the
   SPE-specific registers, and place the results in EVRREGSET.  If we
   don't support PTRACE_GETEVRREGS, then just fill EVRREGSET with
   zeros.

   All the logic to deal with whether or not the PTRACE_GETEVRREGS and
   PTRACE_SETEVRREGS requests are supported is isolated here, and in
   set_spe_registers.  */
static void
get_spe_registers (int tid, struct gdb_evrregset_t *evrregset)
{
  if (have_ptrace_getsetevrregs)
    {
      if (ptrace (PTRACE_GETEVRREGS, tid, 0, evrregset) >= 0)
        return;
      else
        {
          /* EIO means that the PTRACE_GETEVRREGS request isn't supported;
             we just return zeros.  */
          if (errno == EIO)
            have_ptrace_getsetevrregs = 0;
          else
            /* Anything else needs to be reported.  */
            perror_with_name ("Unable to fetch SPE registers");
        }
    }

  memset (evrregset, 0, sizeof (*evrregset));
}

/* Supply values from TID for SPE-specific raw registers: the upper
   halves of the GPRs, the accumulator, and the spefscr.  REGNO must
   be the number of an upper half register, acc, spefscr, or -1 to
   supply the values of all registers.  */
static void
fetch_spe_register (int tid, int regno)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  struct gdb_evrregset_t evrregs;

  gdb_assert (sizeof (evrregs.evr[0])
              == register_size (current_gdbarch, tdep->ppc_ev0_upper_regnum));
  gdb_assert (sizeof (evrregs.acc)
              == register_size (current_gdbarch, tdep->ppc_acc_regnum));
  gdb_assert (sizeof (evrregs.spefscr)
              == register_size (current_gdbarch, tdep->ppc_spefscr_regnum));

  get_spe_registers (tid, &evrregs);

  if (regno == -1)
    {
      int i;

      for (i = 0; i < ppc_num_gprs; i++)
        regcache_raw_supply (current_regcache, tdep->ppc_ev0_upper_regnum + i,
                             &evrregs.evr[i]);
    }
  else if (tdep->ppc_ev0_upper_regnum <= regno
           && regno < tdep->ppc_ev0_upper_regnum + ppc_num_gprs)
    regcache_raw_supply (current_regcache, regno,
                         &evrregs.evr[regno - tdep->ppc_ev0_upper_regnum]);

  if (regno == -1
      || regno == tdep->ppc_acc_regnum)
    regcache_raw_supply (current_regcache, tdep->ppc_acc_regnum, &evrregs.acc);

  if (regno == -1
      || regno == tdep->ppc_spefscr_regnum)
    regcache_raw_supply (current_regcache, tdep->ppc_spefscr_regnum,
                         &evrregs.spefscr);
}

static void
fetch_register (int tid, int regno)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  /* This isn't really an address.  But ptrace thinks of it as one.  */
  CORE_ADDR regaddr = ppc_register_u_addr (regno);
  int bytes_transferred;
  unsigned int offset;         /* Offset of registers within the u area. */
  char buf[MAX_REGISTER_SIZE];

  if (altivec_register_p (regno))
    {
      /* If this is the first time through, or if it is not the first
         time through, and we have comfirmed that there is kernel
         support for such a ptrace request, then go and fetch the
         register.  */
      if (have_ptrace_getvrregs)
       {
         fetch_altivec_register (tid, regno);
         return;
       }
     /* If we have discovered that there is no ptrace support for
        AltiVec registers, fall through and return zeroes, because
        regaddr will be -1 in this case.  */
    }
  else if (spe_register_p (regno))
    {
      fetch_spe_register (tid, regno);
      return;
    }

  if (regaddr == -1)
    {
      memset (buf, '\0', register_size (current_gdbarch, regno));   /* Supply zeroes */
      regcache_raw_supply (current_regcache, regno, buf);
      return;
    }

  /* Read the raw register using PTRACE_XFER_TYPE sized chunks.  On a
     32-bit platform, 64-bit floating-point registers will require two
     transfers.  */
  for (bytes_transferred = 0;
       bytes_transferred < register_size (current_gdbarch, regno);
       bytes_transferred += sizeof (PTRACE_XFER_TYPE))
    {
      errno = 0;
      *(PTRACE_XFER_TYPE *) & buf[bytes_transferred]
        = ptrace (PT_READ_U, tid, (PTRACE_ARG3_TYPE) regaddr, 0);
      regaddr += sizeof (PTRACE_XFER_TYPE);
      if (errno != 0)
	{
          char message[128];
	  sprintf (message, "reading register %s (#%d)", 
		   REGISTER_NAME (regno), regno);
	  perror_with_name (message);
	}
    }

  /* Now supply the register.  Keep in mind that the regcache's idea
     of the register's size may not be a multiple of sizeof
     (PTRACE_XFER_TYPE).  */
  if (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_LITTLE)
    {
      /* Little-endian values are always found at the left end of the
         bytes transferred.  */
      regcache_raw_supply (current_regcache, regno, buf);
    }
  else if (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_BIG)
    {
      /* Big-endian values are found at the right end of the bytes
         transferred.  */
      size_t padding = (bytes_transferred
                        - register_size (current_gdbarch, regno));
      regcache_raw_supply (current_regcache, regno, buf + padding);
    }
  else 
    internal_error (__FILE__, __LINE__,
                    "fetch_register: unexpected byte order: %d",
                    gdbarch_byte_order (current_gdbarch));
}

static void
supply_vrregset (gdb_vrregset_t *vrregsetp)
{
  int i;
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  int num_of_vrregs = tdep->ppc_vrsave_regnum - tdep->ppc_vr0_regnum + 1;
  int vrregsize = register_size (current_gdbarch, tdep->ppc_vr0_regnum);
  int offset = vrregsize - register_size (current_gdbarch, tdep->ppc_vrsave_regnum);

  for (i = 0; i < num_of_vrregs; i++)
    {
      /* The last 2 registers of this set are only 32 bit long, not
         128.  However an offset is necessary only for VSCR because it
         occupies a whole vector, while VRSAVE occupies a full 4 bytes
         slot.  */
      if (i == (num_of_vrregs - 2))
        regcache_raw_supply (current_regcache, tdep->ppc_vr0_regnum + i,
			     *vrregsetp + i * vrregsize + offset);
      else
        regcache_raw_supply (current_regcache, tdep->ppc_vr0_regnum + i,
			     *vrregsetp + i * vrregsize);
    }
}

static void
fetch_altivec_registers (int tid)
{
  int ret;
  gdb_vrregset_t regs;
  
  ret = ptrace (PTRACE_GETVRREGS, tid, 0, &regs);
  if (ret < 0)
    {
      if (errno == EIO)
	{
          have_ptrace_getvrregs = 0;
	  return;
	}
      perror_with_name ("Unable to fetch AltiVec registers");
    }
  supply_vrregset (&regs);
}

static void 
fetch_ppc_registers (int tid)
{
  int i;
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);

  for (i = 0; i < ppc_num_gprs; i++)
    fetch_register (tid, tdep->ppc_gp0_regnum + i);
  if (tdep->ppc_fp0_regnum >= 0)
    for (i = 0; i < ppc_num_fprs; i++)
      fetch_register (tid, tdep->ppc_fp0_regnum + i);
  fetch_register (tid, PC_REGNUM);
  if (tdep->ppc_ps_regnum != -1)
    fetch_register (tid, tdep->ppc_ps_regnum);
  if (tdep->ppc_cr_regnum != -1)
    fetch_register (tid, tdep->ppc_cr_regnum);
  if (tdep->ppc_lr_regnum != -1)
    fetch_register (tid, tdep->ppc_lr_regnum);
  if (tdep->ppc_ctr_regnum != -1)
    fetch_register (tid, tdep->ppc_ctr_regnum);
  if (tdep->ppc_xer_regnum != -1)
    fetch_register (tid, tdep->ppc_xer_regnum);
  if (tdep->ppc_mq_regnum != -1)
    fetch_register (tid, tdep->ppc_mq_regnum);
  if (tdep->ppc_fpscr_regnum != -1)
    fetch_register (tid, tdep->ppc_fpscr_regnum);
  if (have_ptrace_getvrregs)
    if (tdep->ppc_vr0_regnum != -1 && tdep->ppc_vrsave_regnum != -1)
      fetch_altivec_registers (tid);
  if (tdep->ppc_ev0_upper_regnum >= 0)
    fetch_spe_register (tid, -1);
}

/* Fetch registers from the child process.  Fetch all registers if
   regno == -1, otherwise fetch all general registers or all floating
   point registers depending upon the value of regno.  */
void
fetch_inferior_registers (int regno)
{
  /* Overload thread id onto process id */
  int tid = TIDGET (inferior_ptid);

  /* No thread id, just use process id */
  if (tid == 0)
    tid = PIDGET (inferior_ptid);

  if (regno == -1)
    fetch_ppc_registers (tid);
  else 
    fetch_register (tid, regno);
}

/* Store one register. */
static void
store_altivec_register (int tid, int regno)
{
  int ret;
  int offset = 0;
  gdb_vrregset_t regs;
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  int vrregsize = register_size (current_gdbarch, tdep->ppc_vr0_regnum);

  ret = ptrace (PTRACE_GETVRREGS, tid, 0, &regs);
  if (ret < 0)
    {
      if (errno == EIO)
        {
          have_ptrace_getvrregs = 0;
          return;
        }
      perror_with_name ("Unable to fetch AltiVec register");
    }

  /* VSCR is fetched as a 16 bytes quantity, but it is really 4 bytes
     long on the hardware.  */
  if (regno == (tdep->ppc_vrsave_regnum - 1))
    offset = vrregsize - register_size (current_gdbarch, tdep->ppc_vrsave_regnum);

  regcache_raw_collect (current_regcache, regno,
			regs + (regno - tdep->ppc_vr0_regnum) * vrregsize + offset);

  ret = ptrace (PTRACE_SETVRREGS, tid, 0, &regs);
  if (ret < 0)
    perror_with_name ("Unable to store AltiVec register");
}

/* Assuming TID referrs to an SPE process, set the top halves of TID's
   general-purpose registers and its SPE-specific registers to the
   values in EVRREGSET.  If we don't support PTRACE_SETEVRREGS, do
   nothing.

   All the logic to deal with whether or not the PTRACE_GETEVRREGS and
   PTRACE_SETEVRREGS requests are supported is isolated here, and in
   get_spe_registers.  */
static void
set_spe_registers (int tid, struct gdb_evrregset_t *evrregset)
{
  if (have_ptrace_getsetevrregs)
    {
      if (ptrace (PTRACE_SETEVRREGS, tid, 0, evrregset) >= 0)
        return;
      else
        {
          /* EIO means that the PTRACE_SETEVRREGS request isn't
             supported; we fail silently, and don't try the call
             again.  */
          if (errno == EIO)
            have_ptrace_getsetevrregs = 0;
          else
            /* Anything else needs to be reported.  */
            perror_with_name ("Unable to set SPE registers");
        }
    }
}

/* Write GDB's value for the SPE-specific raw register REGNO to TID.
   If REGNO is -1, write the values of all the SPE-specific
   registers.  */
static void
store_spe_register (int tid, int regno)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  struct gdb_evrregset_t evrregs;

  gdb_assert (sizeof (evrregs.evr[0])
              == register_size (current_gdbarch, tdep->ppc_ev0_upper_regnum));
  gdb_assert (sizeof (evrregs.acc)
              == register_size (current_gdbarch, tdep->ppc_acc_regnum));
  gdb_assert (sizeof (evrregs.spefscr)
              == register_size (current_gdbarch, tdep->ppc_spefscr_regnum));

  if (regno == -1)
    /* Since we're going to write out every register, the code below
       should store to every field of evrregs; if that doesn't happen,
       make it obvious by initializing it with suspicious values.  */
    memset (&evrregs, 42, sizeof (evrregs));
  else
    /* We can only read and write the entire EVR register set at a
       time, so to write just a single register, we do a
       read-modify-write maneuver.  */
    get_spe_registers (tid, &evrregs);

  if (regno == -1)
    {
      int i;

      for (i = 0; i < ppc_num_gprs; i++)
        regcache_raw_collect (current_regcache,
                              tdep->ppc_ev0_upper_regnum + i,
                              &evrregs.evr[i]);
    }
  else if (tdep->ppc_ev0_upper_regnum <= regno
           && regno < tdep->ppc_ev0_upper_regnum + ppc_num_gprs)
    regcache_raw_collect (current_regcache, regno,
                          &evrregs.evr[regno - tdep->ppc_ev0_upper_regnum]);

  if (regno == -1
      || regno == tdep->ppc_acc_regnum)
    regcache_raw_collect (current_regcache,
                          tdep->ppc_acc_regnum,
                          &evrregs.acc);

  if (regno == -1
      || regno == tdep->ppc_spefscr_regnum)
    regcache_raw_collect (current_regcache,
                          tdep->ppc_spefscr_regnum,
                          &evrregs.spefscr);

  /* Write back the modified register set.  */
  set_spe_registers (tid, &evrregs);
}

static void
store_register (int tid, int regno)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  /* This isn't really an address.  But ptrace thinks of it as one.  */
  CORE_ADDR regaddr = ppc_register_u_addr (regno);
  int i;
  size_t bytes_to_transfer;
  char buf[MAX_REGISTER_SIZE];

  if (altivec_register_p (regno))
    {
      store_altivec_register (tid, regno);
      return;
    }
  else if (spe_register_p (regno))
    {
      store_spe_register (tid, regno);
      return;
    }

  if (regaddr == -1)
    return;

  /* First collect the register.  Keep in mind that the regcache's
     idea of the register's size may not be a multiple of sizeof
     (PTRACE_XFER_TYPE).  */
  memset (buf, 0, sizeof buf);
  bytes_to_transfer = align_up (register_size (current_gdbarch, regno),
                                sizeof (PTRACE_XFER_TYPE));
  if (TARGET_BYTE_ORDER == BFD_ENDIAN_LITTLE)
    {
      /* Little-endian values always sit at the left end of the buffer.  */
      regcache_raw_collect (current_regcache, regno, buf);
    }
  else if (TARGET_BYTE_ORDER == BFD_ENDIAN_BIG)
    {
      /* Big-endian values sit at the right end of the buffer.  */
      size_t padding = (bytes_to_transfer
                        - register_size (current_gdbarch, regno));
      regcache_raw_collect (current_regcache, regno, buf + padding);
    }

  for (i = 0; i < bytes_to_transfer; i += sizeof (PTRACE_XFER_TYPE))
    {
      errno = 0;
      ptrace (PT_WRITE_U, tid, (PTRACE_ARG3_TYPE) regaddr,
	      *(PTRACE_XFER_TYPE *) & buf[i]);
      regaddr += sizeof (PTRACE_XFER_TYPE);

      if (errno == EIO 
          && regno == tdep->ppc_fpscr_regnum)
	{
	  /* Some older kernel versions don't allow fpscr to be written.  */
	  continue;
	}

      if (errno != 0)
	{
          char message[128];
	  sprintf (message, "writing register %s (#%d)", 
		   REGISTER_NAME (regno), regno);
	  perror_with_name (message);
	}
    }
}

static void
fill_vrregset (gdb_vrregset_t *vrregsetp)
{
  int i;
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  int num_of_vrregs = tdep->ppc_vrsave_regnum - tdep->ppc_vr0_regnum + 1;
  int vrregsize = register_size (current_gdbarch, tdep->ppc_vr0_regnum);
  int offset = vrregsize - register_size (current_gdbarch, tdep->ppc_vrsave_regnum);

  for (i = 0; i < num_of_vrregs; i++)
    {
      /* The last 2 registers of this set are only 32 bit long, not
         128, but only VSCR is fetched as a 16 bytes quantity.  */
      if (i == (num_of_vrregs - 2))
        regcache_raw_collect (current_regcache, tdep->ppc_vr0_regnum + i,
			      *vrregsetp + i * vrregsize + offset);
      else
        regcache_raw_collect (current_regcache, tdep->ppc_vr0_regnum + i,
			      *vrregsetp + i * vrregsize);
    }
}

static void
store_altivec_registers (int tid)
{
  int ret;
  gdb_vrregset_t regs;

  ret = ptrace (PTRACE_GETVRREGS, tid, 0, &regs);
  if (ret < 0)
    {
      if (errno == EIO)
        {
          have_ptrace_getvrregs = 0;
          return;
        }
      perror_with_name ("Couldn't get AltiVec registers");
    }

  fill_vrregset (&regs);
  
  if (ptrace (PTRACE_SETVRREGS, tid, 0, &regs) < 0)
    perror_with_name ("Couldn't write AltiVec registers");
}

static void
store_ppc_registers (int tid)
{
  int i;
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  
  for (i = 0; i < ppc_num_gprs; i++)
    store_register (tid, tdep->ppc_gp0_regnum + i);
  if (tdep->ppc_fp0_regnum >= 0)
    for (i = 0; i < ppc_num_fprs; i++)
      store_register (tid, tdep->ppc_fp0_regnum + i);
  store_register (tid, PC_REGNUM);
  if (tdep->ppc_ps_regnum != -1)
    store_register (tid, tdep->ppc_ps_regnum);
  if (tdep->ppc_cr_regnum != -1)
    store_register (tid, tdep->ppc_cr_regnum);
  if (tdep->ppc_lr_regnum != -1)
    store_register (tid, tdep->ppc_lr_regnum);
  if (tdep->ppc_ctr_regnum != -1)
    store_register (tid, tdep->ppc_ctr_regnum);
  if (tdep->ppc_xer_regnum != -1)
    store_register (tid, tdep->ppc_xer_regnum);
  if (tdep->ppc_mq_regnum != -1)
    store_register (tid, tdep->ppc_mq_regnum);
  if (tdep->ppc_fpscr_regnum != -1)
    store_register (tid, tdep->ppc_fpscr_regnum);
  if (have_ptrace_getvrregs)
    if (tdep->ppc_vr0_regnum != -1 && tdep->ppc_vrsave_regnum != -1)
      store_altivec_registers (tid);
  if (tdep->ppc_ev0_upper_regnum >= 0)
    store_spe_register (tid, -1);
}

void
store_inferior_registers (int regno)
{
  /* Overload thread id onto process id */
  int tid = TIDGET (inferior_ptid);

  /* No thread id, just use process id */
  if (tid == 0)
    tid = PIDGET (inferior_ptid);

  if (regno >= 0)
    store_register (tid, regno);
  else
    store_ppc_registers (tid);
}

void
supply_gregset (gdb_gregset_t *gregsetp)
{
  /* NOTE: cagney/2003-11-25: This is the word size used by the ptrace
     interface, and not the wordsize of the program's ABI.  */
  int wordsize = sizeof (PTRACE_XFER_TYPE);
  ppc_linux_supply_gregset (current_regcache, -1, gregsetp,
			    sizeof (gdb_gregset_t), wordsize);
}

static void
right_fill_reg (int regnum, void *reg)
{
  /* NOTE: cagney/2003-11-25: This is the word size used by the ptrace
     interface, and not the wordsize of the program's ABI.  */
  int wordsize = sizeof (PTRACE_XFER_TYPE);
  /* Right fill the register.  */
  regcache_raw_collect (current_regcache, regnum,
			((bfd_byte *) reg
			 + wordsize
			 - register_size (current_gdbarch, regnum)));
}

void
fill_gregset (gdb_gregset_t *gregsetp, int regno)
{
  int regi;
  elf_greg_t *regp = (elf_greg_t *) gregsetp;
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch); 
  const int elf_ngreg = 48;


  /* Start with zeros.  */
  memset (regp, 0, elf_ngreg * sizeof (*regp));

  for (regi = 0; regi < ppc_num_gprs; regi++)
    {
      if ((regno == -1) || regno == tdep->ppc_gp0_regnum + regi)
	right_fill_reg (tdep->ppc_gp0_regnum + regi, (regp + PT_R0 + regi));
    }

  if ((regno == -1) || regno == PC_REGNUM)
    right_fill_reg (PC_REGNUM, regp + PT_NIP);
  if ((regno == -1) || regno == tdep->ppc_lr_regnum)
    right_fill_reg (tdep->ppc_lr_regnum, regp + PT_LNK);
  if ((regno == -1) || regno == tdep->ppc_cr_regnum)
    regcache_raw_collect (current_regcache, tdep->ppc_cr_regnum,
			  regp + PT_CCR);
  if ((regno == -1) || regno == tdep->ppc_xer_regnum)
    regcache_raw_collect (current_regcache, tdep->ppc_xer_regnum,
			  regp + PT_XER);
  if ((regno == -1) || regno == tdep->ppc_ctr_regnum)
    right_fill_reg (tdep->ppc_ctr_regnum, regp + PT_CTR);
#ifdef PT_MQ
  if (((regno == -1) || regno == tdep->ppc_mq_regnum)
      && (tdep->ppc_mq_regnum != -1))
    right_fill_reg (tdep->ppc_mq_regnum, regp + PT_MQ);
#endif
  if ((regno == -1) || regno == tdep->ppc_ps_regnum)
    right_fill_reg (tdep->ppc_ps_regnum, regp + PT_MSR);
}

void
supply_fpregset (gdb_fpregset_t * fpregsetp)
{
  ppc_linux_supply_fpregset (NULL, current_regcache, -1, fpregsetp,
			     sizeof (gdb_fpregset_t));
}

/* Given a pointer to a floating point register set in /proc format
   (fpregset_t *), update the register specified by REGNO from gdb's
   idea of the current floating point register set.  If REGNO is -1,
   update them all.  */
void
fill_fpregset (gdb_fpregset_t *fpregsetp, int regno)
{
  int regi;
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch); 
  bfd_byte *fpp = (void *) fpregsetp;
  
  if (ppc_floating_point_unit_p (current_gdbarch))
    {
      for (regi = 0; regi < ppc_num_fprs; regi++)
        {
          if ((regno == -1) || (regno == tdep->ppc_fp0_regnum + regi))
            regcache_raw_collect (current_regcache, tdep->ppc_fp0_regnum + regi,
				  fpp + 8 * regi);
        }
      if (regno == -1 || regno == tdep->ppc_fpscr_regnum)
        right_fill_reg (tdep->ppc_fpscr_regnum, (fpp + 8 * 32));
    }
}
