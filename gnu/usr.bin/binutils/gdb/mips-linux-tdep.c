/* Target-dependent code for GNU/Linux on MIPS processors.

   Copyright 2001, 2002, 2004 Free Software Foundation, Inc.

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
#include "target.h"
#include "solib-svr4.h"
#include "osabi.h"
#include "mips-tdep.h"
#include "gdb_string.h"
#include "gdb_assert.h"
#include "frame.h"

/* Copied from <asm/elf.h>.  */
#define ELF_NGREG       45
#define ELF_NFPREG      33

typedef unsigned char elf_greg_t[4];
typedef elf_greg_t elf_gregset_t[ELF_NGREG];

typedef unsigned char elf_fpreg_t[8];
typedef elf_fpreg_t elf_fpregset_t[ELF_NFPREG];

/* 0 - 31 are integer registers, 32 - 63 are fp registers.  */
#define FPR_BASE        32
#define PC              64
#define CAUSE           65
#define BADVADDR        66
#define MMHI            67
#define MMLO            68
#define FPC_CSR         69
#define FPC_EIR         70

#define EF_REG0			6
#define EF_REG31		37
#define EF_LO			38
#define EF_HI			39
#define EF_CP0_EPC		40
#define EF_CP0_BADVADDR		41
#define EF_CP0_STATUS		42
#define EF_CP0_CAUSE		43

#define EF_SIZE			180

/* Figure out where the longjmp will land.
   We expect the first arg to be a pointer to the jmp_buf structure from
   which we extract the pc (MIPS_LINUX_JB_PC) that we will land at.  The pc
   is copied into PC.  This routine returns 1 on success.  */

#define MIPS_LINUX_JB_ELEMENT_SIZE 4
#define MIPS_LINUX_JB_PC 0

static int
mips_linux_get_longjmp_target (CORE_ADDR *pc)
{
  CORE_ADDR jb_addr;
  char buf[TARGET_PTR_BIT / TARGET_CHAR_BIT];

  jb_addr = read_register (A0_REGNUM);

  if (target_read_memory (jb_addr
			  + MIPS_LINUX_JB_PC * MIPS_LINUX_JB_ELEMENT_SIZE,
			  buf, TARGET_PTR_BIT / TARGET_CHAR_BIT))
    return 0;

  *pc = extract_unsigned_integer (buf, TARGET_PTR_BIT / TARGET_CHAR_BIT);

  return 1;
}

/* Transform the bits comprising a 32-bit register to the right size
   for supply_register().  This is needed when mips_regsize() is 8.  */

static void
supply_32bit_reg (int regnum, const void *addr)
{
  char buf[MAX_REGISTER_SIZE];
  store_signed_integer (buf, DEPRECATED_REGISTER_RAW_SIZE (regnum),
                        extract_signed_integer (addr, 4));
  supply_register (regnum, buf);
}

/* Unpack an elf_gregset_t into GDB's register cache.  */

void 
supply_gregset (elf_gregset_t *gregsetp)
{
  int regi;
  elf_greg_t *regp = *gregsetp;
  char zerobuf[MAX_REGISTER_SIZE];

  memset (zerobuf, 0, MAX_REGISTER_SIZE);

  for (regi = EF_REG0; regi <= EF_REG31; regi++)
    supply_32bit_reg ((regi - EF_REG0), (char *)(regp + regi));

  supply_32bit_reg (mips_regnum (current_gdbarch)->lo,
		    (char *)(regp + EF_LO));
  supply_32bit_reg (mips_regnum (current_gdbarch)->hi,
		    (char *)(regp + EF_HI));

  supply_32bit_reg (mips_regnum (current_gdbarch)->pc,
		    (char *)(regp + EF_CP0_EPC));
  supply_32bit_reg (mips_regnum (current_gdbarch)->badvaddr,
		    (char *)(regp + EF_CP0_BADVADDR));
  supply_32bit_reg (PS_REGNUM, (char *)(regp + EF_CP0_STATUS));
  supply_32bit_reg (mips_regnum (current_gdbarch)->cause,
		    (char *)(regp + EF_CP0_CAUSE));

  /* Fill inaccessible registers with zero.  */
  supply_register (UNUSED_REGNUM, zerobuf);
  for (regi = FIRST_EMBED_REGNUM; regi < LAST_EMBED_REGNUM; regi++)
    supply_register (regi, zerobuf);
}

/* Pack our registers (or one register) into an elf_gregset_t.  */

void
fill_gregset (elf_gregset_t *gregsetp, int regno)
{
  int regaddr, regi;
  elf_greg_t *regp = *gregsetp;
  void *dst;

  if (regno == -1)
    {
      memset (regp, 0, sizeof (elf_gregset_t));
      for (regi = 0; regi < 32; regi++)
        fill_gregset (gregsetp, regi);
      fill_gregset (gregsetp, mips_regnum (current_gdbarch)->lo);
      fill_gregset (gregsetp, mips_regnum (current_gdbarch)->hi);
      fill_gregset (gregsetp, mips_regnum (current_gdbarch)->pc);
      fill_gregset (gregsetp, mips_regnum (current_gdbarch)->badvaddr);
      fill_gregset (gregsetp, PS_REGNUM);
      fill_gregset (gregsetp, mips_regnum (current_gdbarch)->cause);

      return;
   }

  if (regno < 32)
    {
      dst = regp + regno + EF_REG0;
      regcache_collect (regno, dst);
      return;
    }

  if (regno == mips_regnum (current_gdbarch)->lo)
    regaddr = EF_LO;
  else if (regno == mips_regnum (current_gdbarch)->hi)
    regaddr = EF_HI;
  else if (regno == mips_regnum (current_gdbarch)->pc)
    regaddr = EF_CP0_EPC;
  else if (regno == mips_regnum (current_gdbarch)->badvaddr)
    regaddr = EF_CP0_BADVADDR;
  else if (regno == PS_REGNUM)
    regaddr = EF_CP0_STATUS;
  else if (regno == mips_regnum (current_gdbarch)->cause)
    regaddr = EF_CP0_CAUSE;
  else
    regaddr = -1;

  if (regaddr != -1)
    {
      dst = regp + regaddr;
      regcache_collect (regno, dst);
    }
}

/* Likewise, unpack an elf_fpregset_t.  */

void
supply_fpregset (elf_fpregset_t *fpregsetp)
{
  int regi;
  char zerobuf[MAX_REGISTER_SIZE];

  memset (zerobuf, 0, MAX_REGISTER_SIZE);

  for (regi = 0; regi < 32; regi++)
    supply_register (FP0_REGNUM + regi,
		     (char *)(*fpregsetp + regi));

  supply_register (mips_regnum (current_gdbarch)->fp_control_status,
		   (char *)(*fpregsetp + 32));

  /* FIXME: how can we supply FCRIR?  The ABI doesn't tell us. */
  supply_register (mips_regnum (current_gdbarch)->fp_implementation_revision,
		   zerobuf);
}

/* Likewise, pack one or all floating point registers into an
   elf_fpregset_t.  */

void
fill_fpregset (elf_fpregset_t *fpregsetp, int regno)
{
  char *from, *to;

  if ((regno >= FP0_REGNUM) && (regno < FP0_REGNUM + 32))
    {
      from = (char *) &deprecated_registers[DEPRECATED_REGISTER_BYTE (regno)];
      to = (char *) (*fpregsetp + regno - FP0_REGNUM);
      memcpy (to, from, DEPRECATED_REGISTER_RAW_SIZE (regno - FP0_REGNUM));
    }
  else if (regno == mips_regnum (current_gdbarch)->fp_control_status)
    {
      from = (char *) &deprecated_registers[DEPRECATED_REGISTER_BYTE (regno)];
      to = (char *) (*fpregsetp + 32);
      memcpy (to, from, DEPRECATED_REGISTER_RAW_SIZE (regno));
    }
  else if (regno == -1)
    {
      int regi;

      for (regi = 0; regi < 32; regi++)
	fill_fpregset (fpregsetp, FP0_REGNUM + regi);
      fill_fpregset(fpregsetp, mips_regnum (current_gdbarch)->fp_control_status);
    }
}

/* Map gdb internal register number to ptrace ``address''.
   These ``addresses'' are normally defined in <asm/ptrace.h>.  */

static CORE_ADDR
mips_linux_register_addr (int regno, CORE_ADDR blockend)
{
  int regaddr;

  if (regno < 0 || regno >= NUM_REGS)
    error ("Bogon register number %d.", regno);

  if (regno < 32)
    regaddr = regno;
  else if ((regno >= mips_regnum (current_gdbarch)->fp0)
	   && (regno < mips_regnum (current_gdbarch)->fp0 + 32))
    regaddr = FPR_BASE + (regno - mips_regnum (current_gdbarch)->fp0);
  else if (regno == mips_regnum (current_gdbarch)->pc)
    regaddr = PC;
  else if (regno == mips_regnum (current_gdbarch)->cause)
    regaddr = CAUSE;
  else if (regno == mips_regnum (current_gdbarch)->badvaddr)
    regaddr = BADVADDR;
  else if (regno == mips_regnum (current_gdbarch)->lo)
    regaddr = MMLO;
  else if (regno == mips_regnum (current_gdbarch)->hi)
    regaddr = MMHI;
  else if (regno == mips_regnum (current_gdbarch)->fp_control_status)
    regaddr = FPC_CSR;
  else if (regno == mips_regnum (current_gdbarch)->fp_implementation_revision)
    regaddr = FPC_EIR;
  else
    error ("Unknowable register number %d.", regno);

  return regaddr;
}


/* Fetch (and possibly build) an appropriate link_map_offsets
   structure for native GNU/Linux MIPS targets using the struct offsets
   defined in link.h (but without actual reference to that file).

   This makes it possible to access GNU/Linux MIPS shared libraries from a
   GDB that was built on a different host platform (for cross debugging).  */

static struct link_map_offsets *
mips_linux_svr4_fetch_link_map_offsets (void)
{ 
  static struct link_map_offsets lmo;
  static struct link_map_offsets *lmp = NULL;

  if (lmp == NULL)
    { 
      lmp = &lmo;

      lmo.r_debug_size = 8;	/* The actual size is 20 bytes, but
				   this is all we need.  */
      lmo.r_map_offset = 4;
      lmo.r_map_size   = 4;

      lmo.link_map_size = 20;

      lmo.l_addr_offset = 0;
      lmo.l_addr_size   = 4;

      lmo.l_name_offset = 4;
      lmo.l_name_size   = 4;

      lmo.l_next_offset = 12;
      lmo.l_next_size   = 4;

      lmo.l_prev_offset = 16;
      lmo.l_prev_size   = 4;
    }

  return lmp;
}

/* Support for 64-bit ABIs.  */

/* Copied from <asm/elf.h>.  */
#define MIPS64_ELF_NGREG       45
#define MIPS64_ELF_NFPREG      33

typedef unsigned char mips64_elf_greg_t[8];
typedef mips64_elf_greg_t mips64_elf_gregset_t[MIPS64_ELF_NGREG];

typedef unsigned char mips64_elf_fpreg_t[8];
typedef mips64_elf_fpreg_t mips64_elf_fpregset_t[MIPS64_ELF_NFPREG];

/* 0 - 31 are integer registers, 32 - 63 are fp registers.  */
#define MIPS64_FPR_BASE                 32
#define MIPS64_PC                       64
#define MIPS64_CAUSE                    65
#define MIPS64_BADVADDR                 66
#define MIPS64_MMHI                     67
#define MIPS64_MMLO                     68
#define MIPS64_FPC_CSR                  69
#define MIPS64_FPC_EIR                  70

#define MIPS64_EF_REG0			 0
#define MIPS64_EF_REG31			31
#define MIPS64_EF_LO			32
#define MIPS64_EF_HI			33
#define MIPS64_EF_CP0_EPC		34
#define MIPS64_EF_CP0_BADVADDR		35
#define MIPS64_EF_CP0_STATUS		36
#define MIPS64_EF_CP0_CAUSE		37

#define MIPS64_EF_SIZE			304

/* Figure out where the longjmp will land.
   We expect the first arg to be a pointer to the jmp_buf structure from
   which we extract the pc (MIPS_LINUX_JB_PC) that we will land at.  The pc
   is copied into PC.  This routine returns 1 on success.  */

/* Details about jmp_buf.  */

#define MIPS64_LINUX_JB_PC 0

static int
mips64_linux_get_longjmp_target (CORE_ADDR *pc)
{
  CORE_ADDR jb_addr;
  void *buf = alloca (TARGET_PTR_BIT / TARGET_CHAR_BIT);
  int element_size = TARGET_PTR_BIT == 32 ? 4 : 8;

  jb_addr = read_register (A0_REGNUM);

  if (target_read_memory (jb_addr + MIPS64_LINUX_JB_PC * element_size,
			  buf, TARGET_PTR_BIT / TARGET_CHAR_BIT))
    return 0;

  *pc = extract_unsigned_integer (buf, TARGET_PTR_BIT / TARGET_CHAR_BIT);

  return 1;
}

/* Unpack an elf_gregset_t into GDB's register cache.  */

static void 
mips64_supply_gregset (mips64_elf_gregset_t *gregsetp)
{
  int regi;
  mips64_elf_greg_t *regp = *gregsetp;
  char zerobuf[MAX_REGISTER_SIZE];

  memset (zerobuf, 0, MAX_REGISTER_SIZE);

  for (regi = MIPS64_EF_REG0; regi <= MIPS64_EF_REG31; regi++)
    supply_register ((regi - MIPS64_EF_REG0), (char *)(regp + regi));

  supply_register (mips_regnum (current_gdbarch)->lo,
		   (char *)(regp + MIPS64_EF_LO));
  supply_register (mips_regnum (current_gdbarch)->hi,
		   (char *)(regp + MIPS64_EF_HI));

  supply_register (mips_regnum (current_gdbarch)->pc,
		   (char *)(regp + MIPS64_EF_CP0_EPC));
  supply_register (mips_regnum (current_gdbarch)->badvaddr,
		   (char *)(regp + MIPS64_EF_CP0_BADVADDR));
  supply_register (PS_REGNUM, (char *)(regp + MIPS64_EF_CP0_STATUS));
  supply_register (mips_regnum (current_gdbarch)->cause,
		   (char *)(regp + MIPS64_EF_CP0_CAUSE));

  /* Fill inaccessible registers with zero.  */
  supply_register (UNUSED_REGNUM, zerobuf);
  for (regi = FIRST_EMBED_REGNUM; regi < LAST_EMBED_REGNUM; regi++)
    supply_register (regi, zerobuf);
}

/* Pack our registers (or one register) into an elf_gregset_t.  */

static void
mips64_fill_gregset (mips64_elf_gregset_t *gregsetp, int regno)
{
  int regaddr, regi;
  mips64_elf_greg_t *regp = *gregsetp;
  void *src, *dst;

  if (regno == -1)
    {
      memset (regp, 0, sizeof (mips64_elf_gregset_t));
      for (regi = 0; regi < 32; regi++)
        mips64_fill_gregset (gregsetp, regi);
      mips64_fill_gregset (gregsetp, mips_regnum (current_gdbarch)->lo);
      mips64_fill_gregset (gregsetp, mips_regnum (current_gdbarch)->hi);
      mips64_fill_gregset (gregsetp, mips_regnum (current_gdbarch)->pc);
      mips64_fill_gregset (gregsetp, mips_regnum (current_gdbarch)->badvaddr);
      mips64_fill_gregset (gregsetp, PS_REGNUM);
      mips64_fill_gregset (gregsetp, mips_regnum (current_gdbarch)->cause);

      return;
   }

  if (regno < 32)
    {
      dst = regp + regno + MIPS64_EF_REG0;
      regcache_collect (regno, dst);
      return;
    }

  if (regno == mips_regnum (current_gdbarch)->lo)
    regaddr = MIPS64_EF_LO;
  else if (regno == mips_regnum (current_gdbarch)->hi)
    regaddr = MIPS64_EF_HI;
  else if (regno == mips_regnum (current_gdbarch)->pc)
    regaddr = MIPS64_EF_CP0_EPC;
  else if (regno == mips_regnum (current_gdbarch)->badvaddr)
    regaddr = MIPS64_EF_CP0_BADVADDR;
  else if (regno == PS_REGNUM)
    regaddr = MIPS64_EF_CP0_STATUS;
  else if (regno == mips_regnum (current_gdbarch)->cause)
    regaddr = MIPS64_EF_CP0_CAUSE;
  else
    regaddr = -1;

  if (regaddr != -1)
    {
      dst = regp + regaddr;
      regcache_collect (regno, dst);
    }
}

/* Likewise, unpack an elf_fpregset_t.  */

static void
mips64_supply_fpregset (mips64_elf_fpregset_t *fpregsetp)
{
  int regi;
  char zerobuf[MAX_REGISTER_SIZE];

  memset (zerobuf, 0, MAX_REGISTER_SIZE);

  for (regi = 0; regi < 32; regi++)
    supply_register (FP0_REGNUM + regi,
		     (char *)(*fpregsetp + regi));

  supply_register (mips_regnum (current_gdbarch)->fp_control_status,
		   (char *)(*fpregsetp + 32));

  /* FIXME: how can we supply FCRIR?  The ABI doesn't tell us. */
  supply_register (mips_regnum (current_gdbarch)->fp_implementation_revision,
		   zerobuf);
}

/* Likewise, pack one or all floating point registers into an
   elf_fpregset_t.  */

static void
mips64_fill_fpregset (mips64_elf_fpregset_t *fpregsetp, int regno)
{
  char *from, *to;

  if ((regno >= FP0_REGNUM) && (regno < FP0_REGNUM + 32))
    {
      from = (char *) &deprecated_registers[DEPRECATED_REGISTER_BYTE (regno)];
      to = (char *) (*fpregsetp + regno - FP0_REGNUM);
      memcpy (to, from, DEPRECATED_REGISTER_RAW_SIZE (regno - FP0_REGNUM));
    }
  else if (regno == mips_regnum (current_gdbarch)->fp_control_status)
    {
      from = (char *) &deprecated_registers[DEPRECATED_REGISTER_BYTE (regno)];
      to = (char *) (*fpregsetp + 32);
      memcpy (to, from, DEPRECATED_REGISTER_RAW_SIZE (regno));
    }
  else if (regno == -1)
    {
      int regi;

      for (regi = 0; regi < 32; regi++)
	mips64_fill_fpregset (fpregsetp, FP0_REGNUM + regi);
      mips64_fill_fpregset(fpregsetp,
			   mips_regnum (current_gdbarch)->fp_control_status);
    }
}


/* Map gdb internal register number to ptrace ``address''.
   These ``addresses'' are normally defined in <asm/ptrace.h>.  */

static CORE_ADDR
mips64_linux_register_addr (int regno, CORE_ADDR blockend)
{
  int regaddr;

  if (regno < 0 || regno >= NUM_REGS)
    error ("Bogon register number %d.", regno);

  if (regno < 32)
    regaddr = regno;
  else if ((regno >= mips_regnum (current_gdbarch)->fp0)
	   && (regno < mips_regnum (current_gdbarch)->fp0 + 32))
    regaddr = MIPS64_FPR_BASE + (regno - FP0_REGNUM);
  else if (regno == mips_regnum (current_gdbarch)->pc)
    regaddr = MIPS64_PC;
  else if (regno == mips_regnum (current_gdbarch)->cause)
    regaddr = MIPS64_CAUSE;
  else if (regno == mips_regnum (current_gdbarch)->badvaddr)
    regaddr = MIPS64_BADVADDR;
  else if (regno == mips_regnum (current_gdbarch)->lo)
    regaddr = MIPS64_MMLO;
  else if (regno == mips_regnum (current_gdbarch)->hi)
    regaddr = MIPS64_MMHI;
  else if (regno == mips_regnum (current_gdbarch)->fp_control_status)
    regaddr = MIPS64_FPC_CSR;
  else if (regno == mips_regnum (current_gdbarch)->fp_implementation_revision)
    regaddr = MIPS64_FPC_EIR;
  else
    error ("Unknowable register number %d.", regno);

  return regaddr;
}

/*  Use a local version of this function to get the correct types for
    regsets, until multi-arch core support is ready.  */

static void
fetch_core_registers (char *core_reg_sect, unsigned core_reg_size,
		      int which, CORE_ADDR reg_addr)
{
  elf_gregset_t gregset;
  elf_fpregset_t fpregset;
  mips64_elf_gregset_t gregset64;
  mips64_elf_fpregset_t fpregset64;

  if (which == 0)
    {
      if (core_reg_size == sizeof (gregset))
	{
	  memcpy ((char *) &gregset, core_reg_sect, sizeof (gregset));
	  supply_gregset (&gregset);
	}
      else if (core_reg_size == sizeof (gregset64))
	{
	  memcpy ((char *) &gregset64, core_reg_sect, sizeof (gregset64));
	  mips64_supply_gregset (&gregset64);
	}
      else
	{
	  warning ("wrong size gregset struct in core file");
	}
    }
  else if (which == 2)
    {
      if (core_reg_size == sizeof (fpregset))
	{
	  memcpy ((char *) &fpregset, core_reg_sect, sizeof (fpregset));
	  supply_fpregset (&fpregset);
	}
      else if (core_reg_size == sizeof (fpregset64))
	{
	  memcpy ((char *) &fpregset64, core_reg_sect, sizeof (fpregset64));
	  mips64_supply_fpregset (&fpregset64);
	}
      else
	{
	  warning ("wrong size fpregset struct in core file");
	}
    }
}

/* Register that we are able to handle ELF file formats using standard
   procfs "regset" structures.  */

static struct core_fns regset_core_fns =
{
  bfd_target_elf_flavour,		/* core_flavour */
  default_check_format,			/* check_format */
  default_core_sniffer,			/* core_sniffer */
  fetch_core_registers,			/* core_read_registers */
  NULL					/* next */
};

/* Fetch (and possibly build) an appropriate link_map_offsets
   structure for native GNU/Linux MIPS targets using the struct offsets
   defined in link.h (but without actual reference to that file).

   This makes it possible to access GNU/Linux MIPS shared libraries from a
   GDB that was built on a different host platform (for cross debugging).  */

static struct link_map_offsets *
mips64_linux_svr4_fetch_link_map_offsets (void)
{ 
  static struct link_map_offsets lmo;
  static struct link_map_offsets *lmp = NULL;

  if (lmp == NULL)
    { 
      lmp = &lmo;

      lmo.r_debug_size = 16;	/* The actual size is 40 bytes, but
				   this is all we need.  */
      lmo.r_map_offset = 8;
      lmo.r_map_size   = 8;

      lmo.link_map_size = 40;

      lmo.l_addr_offset = 0;
      lmo.l_addr_size   = 8;

      lmo.l_name_offset = 8;
      lmo.l_name_size   = 8;

      lmo.l_next_offset = 24;
      lmo.l_next_size   = 8;

      lmo.l_prev_offset = 32;
      lmo.l_prev_size   = 8;
    }

  return lmp;
}

/* Handle for obtaining pointer to the current register_addr() function
   for a given architecture.  */
static struct gdbarch_data *register_addr_data;

CORE_ADDR
register_addr (int regno, CORE_ADDR blockend)
{
  CORE_ADDR (*register_addr_ptr) (int, CORE_ADDR) =
    gdbarch_data (current_gdbarch, register_addr_data);

  gdb_assert (register_addr_ptr != 0);

  return register_addr_ptr (regno, blockend);
}

static void
set_mips_linux_register_addr (struct gdbarch *gdbarch,
                              CORE_ADDR (*register_addr_ptr) (int, CORE_ADDR))
{
  set_gdbarch_data (gdbarch, register_addr_data, register_addr_ptr);
}

static void *
init_register_addr_data (struct gdbarch *gdbarch)
{
  return 0;
}

/* Check the code at PC for a dynamic linker lazy resolution stub.  Because
   they aren't in the .plt section, we pattern-match on the code generated
   by GNU ld.  They look like this:

   lw t9,0x8010(gp)
   addu t7,ra
   jalr t9,ra
   addiu t8,zero,INDEX

   (with the appropriate doubleword instructions for N64).  Also return the
   dynamic symbol index used in the last instruction.  */

static int
mips_linux_in_dynsym_stub (CORE_ADDR pc, char *name)
{
  unsigned char buf[28], *p;
  ULONGEST insn, insn1;
  int n64 = (mips_abi (current_gdbarch) == MIPS_ABI_N64);

  read_memory (pc - 12, buf, 28);

  if (n64)
    {
      /* ld t9,0x8010(gp) */
      insn1 = 0xdf998010;
    }
  else
    {
      /* lw t9,0x8010(gp) */
      insn1 = 0x8f998010;
    }

  p = buf + 12;
  while (p >= buf)
    {
      insn = extract_unsigned_integer (p, 4);
      if (insn == insn1)
	break;
      p -= 4;
    }
  if (p < buf)
    return 0;

  insn = extract_unsigned_integer (p + 4, 4);
  if (n64)
    {
      /* daddu t7,ra */
      if (insn != 0x03e0782d)
	return 0;
    }
  else
    {
      /* addu t7,ra */
      if (insn != 0x03e07821)
	return 0;
    }
  
  insn = extract_unsigned_integer (p + 8, 4);
  /* jalr t9,ra */
  if (insn != 0x0320f809)
    return 0;

  insn = extract_unsigned_integer (p + 12, 4);
  if (n64)
    {
      /* daddiu t8,zero,0 */
      if ((insn & 0xffff0000) != 0x64180000)
	return 0;
    }
  else
    {
      /* addiu t8,zero,0 */
      if ((insn & 0xffff0000) != 0x24180000)
	return 0;
    }

  return (insn & 0xffff);
}

/* Return non-zero iff PC belongs to the dynamic linker resolution code
   or to a stub.  */

int
mips_linux_in_dynsym_resolve_code (CORE_ADDR pc)
{
  /* Check whether PC is in the dynamic linker.  This also checks whether
     it is in the .plt section, which MIPS does not use.  */
  if (in_solib_dynsym_resolve_code (pc))
    return 1;

  /* Pattern match for the stub.  It would be nice if there were a more
     efficient way to avoid this check.  */
  if (mips_linux_in_dynsym_stub (pc, NULL))
    return 1;

  return 0;
}

/* See the comments for SKIP_SOLIB_RESOLVER at the top of infrun.c,
   and glibc_skip_solib_resolver in glibc-tdep.c.  The normal glibc
   implementation of this triggers at "fixup" from the same objfile as
   "_dl_runtime_resolve"; MIPS GNU/Linux can trigger at
   "__dl_runtime_resolve" directly.  An unresolved PLT entry will
   point to _dl_runtime_resolve, which will first call
   __dl_runtime_resolve, and then pass control to the resolved
   function.  */

static CORE_ADDR
mips_linux_skip_resolver (struct gdbarch *gdbarch, CORE_ADDR pc)
{
  struct minimal_symbol *resolver;

  resolver = lookup_minimal_symbol ("__dl_runtime_resolve", NULL, NULL);

  if (resolver && SYMBOL_VALUE_ADDRESS (resolver) == pc)
    return frame_pc_unwind (get_current_frame ()); 

  return 0;
}      

static void
mips_linux_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  enum mips_abi abi = mips_abi (gdbarch);

  switch (abi)
    {
      case MIPS_ABI_O32:
	set_gdbarch_get_longjmp_target (gdbarch,
	                                mips_linux_get_longjmp_target);
	set_solib_svr4_fetch_link_map_offsets
	  (gdbarch, mips_linux_svr4_fetch_link_map_offsets);
	set_mips_linux_register_addr (gdbarch, mips_linux_register_addr);
	break;
      case MIPS_ABI_N32:
	set_gdbarch_get_longjmp_target (gdbarch,
	                                mips_linux_get_longjmp_target);
	set_solib_svr4_fetch_link_map_offsets
	  (gdbarch, mips_linux_svr4_fetch_link_map_offsets);
	set_mips_linux_register_addr (gdbarch, mips64_linux_register_addr);
	break;
      case MIPS_ABI_N64:
	set_gdbarch_get_longjmp_target (gdbarch,
	                                mips64_linux_get_longjmp_target);
	set_solib_svr4_fetch_link_map_offsets
	  (gdbarch, mips64_linux_svr4_fetch_link_map_offsets);
	set_mips_linux_register_addr (gdbarch, mips64_linux_register_addr);
	break;
      default:
	internal_error (__FILE__, __LINE__, "can't handle ABI");
	break;
    }

  set_gdbarch_skip_solib_resolver (gdbarch, mips_linux_skip_resolver);

  /* This overrides the MIPS16 stub support from mips-tdep.  But no
     one uses MIPS16 on GNU/Linux yet, so this isn't much of a loss.  */
  set_gdbarch_in_solib_call_trampoline (gdbarch, mips_linux_in_dynsym_stub);
}

void
_initialize_mips_linux_tdep (void)
{
  const struct bfd_arch_info *arch_info;

  register_addr_data =
    register_gdbarch_data (init_register_addr_data);

  for (arch_info = bfd_lookup_arch (bfd_arch_mips, 0);
       arch_info != NULL;
       arch_info = arch_info->next)
    {
      gdbarch_register_osabi (bfd_arch_mips, arch_info->mach, GDB_OSABI_LINUX,
			      mips_linux_init_abi);
    }

  add_core_fns (&regset_core_fns);
}
