/* Target-dependent code for HPUX running on PA-RISC, for GDB.

   Copyright 2002, 2003 Free Software Foundation, Inc.

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "arch-utils.h"
#include "gdbcore.h"
#include "osabi.h"
#include "gdb_string.h"
#include "frame.h"
#include "frame-unwind.h"
#include "trad-frame.h"
#include "symtab.h"
#include "objfiles.h"
#include "inferior.h"
#include "infcall.h"
#include "observer.h"
#include "hppa-tdep.h"

#include <dl.h>
#include <machine/save_state.h>

#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((unsigned long) &((TYPE *)0)->MEMBER)
#endif

/* Forward declarations.  */
extern void _initialize_hppa_hpux_tdep (void);
extern initialize_file_ftype _initialize_hppa_hpux_tdep;

typedef struct
  {
    struct minimal_symbol *msym;
    CORE_ADDR solib_handle;
    CORE_ADDR return_val;
  }
args_for_find_stub;

/* Return one if PC is in the call path of a trampoline, else return zero.

   Note we return one for *any* call trampoline (long-call, arg-reloc), not
   just shared library trampolines (import, export).  */

static int
hppa32_hpux_in_solib_call_trampoline (CORE_ADDR pc, char *name)
{
  struct minimal_symbol *minsym;
  struct unwind_table_entry *u;

  /* First see if PC is in one of the two C-library trampolines.  */
  if (pc == hppa_symbol_address("$$dyncall") 
      || pc == hppa_symbol_address("_sr4export"))
    return 1;

  minsym = lookup_minimal_symbol_by_pc (pc);
  if (minsym && strcmp (DEPRECATED_SYMBOL_NAME (minsym), ".stub") == 0)
    return 1;

  /* Get the unwind descriptor corresponding to PC, return zero
     if no unwind was found.  */
  u = find_unwind_entry (pc);
  if (!u)
    return 0;

  /* If this isn't a linker stub, then return now.  */
  if (u->stub_unwind.stub_type == 0)
    return 0;

  /* By definition a long-branch stub is a call stub.  */
  if (u->stub_unwind.stub_type == LONG_BRANCH)
    return 1;

  /* The call and return path execute the same instructions within
     an IMPORT stub!  So an IMPORT stub is both a call and return
     trampoline.  */
  if (u->stub_unwind.stub_type == IMPORT)
    return 1;

  /* Parameter relocation stubs always have a call path and may have a
     return path.  */
  if (u->stub_unwind.stub_type == PARAMETER_RELOCATION
      || u->stub_unwind.stub_type == EXPORT)
    {
      CORE_ADDR addr;

      /* Search forward from the current PC until we hit a branch
         or the end of the stub.  */
      for (addr = pc; addr <= u->region_end; addr += 4)
	{
	  unsigned long insn;

	  insn = read_memory_integer (addr, 4);

	  /* Does it look like a bl?  If so then it's the call path, if
	     we find a bv or be first, then we're on the return path.  */
	  if ((insn & 0xfc00e000) == 0xe8000000)
	    return 1;
	  else if ((insn & 0xfc00e001) == 0xe800c000
		   || (insn & 0xfc000000) == 0xe0000000)
	    return 0;
	}

      /* Should never happen.  */
      warning ("Unable to find branch in parameter relocation stub.\n");
      return 0;
    }

  /* Unknown stub type.  For now, just return zero.  */
  return 0;
}

static int
hppa64_hpux_in_solib_call_trampoline (CORE_ADDR pc, char *name)
{
  /* PA64 has a completely different stub/trampoline scheme.  Is it
     better?  Maybe.  It's certainly harder to determine with any
     certainty that we are in a stub because we can not refer to the
     unwinders to help. 

     The heuristic is simple.  Try to lookup the current PC value in th
     minimal symbol table.  If that fails, then assume we are not in a
     stub and return.

     Then see if the PC value falls within the section bounds for the
     section containing the minimal symbol we found in the first
     step.  If it does, then assume we are not in a stub and return.

     Finally peek at the instructions to see if they look like a stub.  */
  struct minimal_symbol *minsym;
  asection *sec;
  CORE_ADDR addr;
  int insn, i;

  minsym = lookup_minimal_symbol_by_pc (pc);
  if (! minsym)
    return 0;

  sec = SYMBOL_BFD_SECTION (minsym);

  if (bfd_get_section_vma (sec->owner, sec) <= pc
      && pc < (bfd_get_section_vma (sec->owner, sec)
		 + bfd_section_size (sec->owner, sec)))
      return 0;

  /* We might be in a stub.  Peek at the instructions.  Stubs are 3
     instructions long. */
  insn = read_memory_integer (pc, 4);

  /* Find out where we think we are within the stub.  */
  if ((insn & 0xffffc00e) == 0x53610000)
    addr = pc;
  else if ((insn & 0xffffffff) == 0xe820d000)
    addr = pc - 4;
  else if ((insn & 0xffffc00e) == 0x537b0000)
    addr = pc - 8;
  else
    return 0;

  /* Now verify each insn in the range looks like a stub instruction.  */
  insn = read_memory_integer (addr, 4);
  if ((insn & 0xffffc00e) != 0x53610000)
    return 0;
	
  /* Now verify each insn in the range looks like a stub instruction.  */
  insn = read_memory_integer (addr + 4, 4);
  if ((insn & 0xffffffff) != 0xe820d000)
    return 0;
    
  /* Now verify each insn in the range looks like a stub instruction.  */
  insn = read_memory_integer (addr + 8, 4);
  if ((insn & 0xffffc00e) != 0x537b0000)
    return 0;

  /* Looks like a stub.  */
  return 1;
}

/* Return one if PC is in the return path of a trampoline, else return zero.

   Note we return one for *any* call trampoline (long-call, arg-reloc), not
   just shared library trampolines (import, export).  */

static int
hppa_hpux_in_solib_return_trampoline (CORE_ADDR pc, char *name)
{
  struct unwind_table_entry *u;

  /* Get the unwind descriptor corresponding to PC, return zero
     if no unwind was found.  */
  u = find_unwind_entry (pc);
  if (!u)
    return 0;

  /* If this isn't a linker stub or it's just a long branch stub, then
     return zero.  */
  if (u->stub_unwind.stub_type == 0 || u->stub_unwind.stub_type == LONG_BRANCH)
    return 0;

  /* The call and return path execute the same instructions within
     an IMPORT stub!  So an IMPORT stub is both a call and return
     trampoline.  */
  if (u->stub_unwind.stub_type == IMPORT)
    return 1;

  /* Parameter relocation stubs always have a call path and may have a
     return path.  */
  if (u->stub_unwind.stub_type == PARAMETER_RELOCATION
      || u->stub_unwind.stub_type == EXPORT)
    {
      CORE_ADDR addr;

      /* Search forward from the current PC until we hit a branch
         or the end of the stub.  */
      for (addr = pc; addr <= u->region_end; addr += 4)
	{
	  unsigned long insn;

	  insn = read_memory_integer (addr, 4);

	  /* Does it look like a bl?  If so then it's the call path, if
	     we find a bv or be first, then we're on the return path.  */
	  if ((insn & 0xfc00e000) == 0xe8000000)
	    return 0;
	  else if ((insn & 0xfc00e001) == 0xe800c000
		   || (insn & 0xfc000000) == 0xe0000000)
	    return 1;
	}

      /* Should never happen.  */
      warning ("Unable to find branch in parameter relocation stub.\n");
      return 0;
    }

  /* Unknown stub type.  For now, just return zero.  */
  return 0;

}

/* Figure out if PC is in a trampoline, and if so find out where
   the trampoline will jump to.  If not in a trampoline, return zero.

   Simple code examination probably is not a good idea since the code
   sequences in trampolines can also appear in user code.

   We use unwinds and information from the minimal symbol table to
   determine when we're in a trampoline.  This won't work for ELF
   (yet) since it doesn't create stub unwind entries.  Whether or
   not ELF will create stub unwinds or normal unwinds for linker
   stubs is still being debated.

   This should handle simple calls through dyncall or sr4export,
   long calls, argument relocation stubs, and dyncall/sr4export
   calling an argument relocation stub.  It even handles some stubs
   used in dynamic executables.  */

static CORE_ADDR
hppa_hpux_skip_trampoline_code (CORE_ADDR pc)
{
  long orig_pc = pc;
  long prev_inst, curr_inst, loc;
  struct minimal_symbol *msym;
  struct unwind_table_entry *u;

  /* Addresses passed to dyncall may *NOT* be the actual address
     of the function.  So we may have to do something special.  */
  if (pc == hppa_symbol_address("$$dyncall"))
    {
      pc = (CORE_ADDR) read_register (22);

      /* If bit 30 (counting from the left) is on, then pc is the address of
         the PLT entry for this function, not the address of the function
         itself.  Bit 31 has meaning too, but only for MPE.  */
      if (pc & 0x2)
	pc = (CORE_ADDR) read_memory_integer (pc & ~0x3, TARGET_PTR_BIT / 8);
    }
  if (pc == hppa_symbol_address("$$dyncall_external"))
    {
      pc = (CORE_ADDR) read_register (22);
      pc = (CORE_ADDR) read_memory_integer (pc & ~0x3, TARGET_PTR_BIT / 8);
    }
  else if (pc == hppa_symbol_address("_sr4export"))
    pc = (CORE_ADDR) (read_register (22));

  /* Get the unwind descriptor corresponding to PC, return zero
     if no unwind was found.  */
  u = find_unwind_entry (pc);
  if (!u)
    return 0;

  /* If this isn't a linker stub, then return now.  */
  /* elz: attention here! (FIXME) because of a compiler/linker 
     error, some stubs which should have a non zero stub_unwind.stub_type 
     have unfortunately a value of zero. So this function would return here
     as if we were not in a trampoline. To fix this, we go look at the partial
     symbol information, which reports this guy as a stub.
     (FIXME): Unfortunately, we are not that lucky: it turns out that the 
     partial symbol information is also wrong sometimes. This is because 
     when it is entered (somread.c::som_symtab_read()) it can happen that
     if the type of the symbol (from the som) is Entry, and the symbol is
     in a shared library, then it can also be a trampoline.  This would
     be OK, except that I believe the way they decide if we are ina shared library
     does not work. SOOOO..., even if we have a regular function w/o trampolines
     its minimal symbol can be assigned type mst_solib_trampoline.
     Also, if we find that the symbol is a real stub, then we fix the unwind
     descriptor, and define the stub type to be EXPORT.
     Hopefully this is correct most of the times. */
  if (u->stub_unwind.stub_type == 0)
    {

/* elz: NOTE (FIXME!) once the problem with the unwind information is fixed
   we can delete all the code which appears between the lines */
/*--------------------------------------------------------------------------*/
      msym = lookup_minimal_symbol_by_pc (pc);

      if (msym == NULL || MSYMBOL_TYPE (msym) != mst_solib_trampoline)
	return orig_pc == pc ? 0 : pc & ~0x3;

      else if (msym != NULL && MSYMBOL_TYPE (msym) == mst_solib_trampoline)
	{
	  struct objfile *objfile;
	  struct minimal_symbol *msymbol;
	  int function_found = 0;

	  /* go look if there is another minimal symbol with the same name as 
	     this one, but with type mst_text. This would happen if the msym
	     is an actual trampoline, in which case there would be another
	     symbol with the same name corresponding to the real function */

	  ALL_MSYMBOLS (objfile, msymbol)
	  {
	    if (MSYMBOL_TYPE (msymbol) == mst_text
		&& DEPRECATED_STREQ (DEPRECATED_SYMBOL_NAME (msymbol), DEPRECATED_SYMBOL_NAME (msym)))
	      {
		function_found = 1;
		break;
	      }
	  }

	  if (function_found)
	    /* the type of msym is correct (mst_solib_trampoline), but
	       the unwind info is wrong, so set it to the correct value */
	    u->stub_unwind.stub_type = EXPORT;
	  else
	    /* the stub type info in the unwind is correct (this is not a
	       trampoline), but the msym type information is wrong, it
	       should be mst_text. So we need to fix the msym, and also
	       get out of this function */
	    {
	      MSYMBOL_TYPE (msym) = mst_text;
	      return orig_pc == pc ? 0 : pc & ~0x3;
	    }
	}

/*--------------------------------------------------------------------------*/
    }

  /* It's a stub.  Search for a branch and figure out where it goes.
     Note we have to handle multi insn branch sequences like ldil;ble.
     Most (all?) other branches can be determined by examining the contents
     of certain registers and the stack.  */

  loc = pc;
  curr_inst = 0;
  prev_inst = 0;
  while (1)
    {
      /* Make sure we haven't walked outside the range of this stub.  */
      if (u != find_unwind_entry (loc))
	{
	  warning ("Unable to find branch in linker stub");
	  return orig_pc == pc ? 0 : pc & ~0x3;
	}

      prev_inst = curr_inst;
      curr_inst = read_memory_integer (loc, 4);

      /* Does it look like a branch external using %r1?  Then it's the
         branch from the stub to the actual function.  */
      if ((curr_inst & 0xffe0e000) == 0xe0202000)
	{
	  /* Yup.  See if the previous instruction loaded
	     a value into %r1.  If so compute and return the jump address.  */
	  if ((prev_inst & 0xffe00000) == 0x20200000)
	    return (hppa_extract_21 (prev_inst) + hppa_extract_17 (curr_inst)) & ~0x3;
	  else
	    {
	      warning ("Unable to find ldil X,%%r1 before ble Y(%%sr4,%%r1).");
	      return orig_pc == pc ? 0 : pc & ~0x3;
	    }
	}

      /* Does it look like a be 0(sr0,%r21)? OR 
         Does it look like a be, n 0(sr0,%r21)? OR 
         Does it look like a bve (r21)? (this is on PA2.0)
         Does it look like a bve, n(r21)? (this is also on PA2.0)
         That's the branch from an
         import stub to an export stub.

         It is impossible to determine the target of the branch via
         simple examination of instructions and/or data (consider
         that the address in the plabel may be the address of the
         bind-on-reference routine in the dynamic loader).

         So we have try an alternative approach.

         Get the name of the symbol at our current location; it should
         be a stub symbol with the same name as the symbol in the
         shared library.

         Then lookup a minimal symbol with the same name; we should
         get the minimal symbol for the target routine in the shared
         library as those take precedence of import/export stubs.  */
      if ((curr_inst == 0xe2a00000) ||
	  (curr_inst == 0xe2a00002) ||
	  (curr_inst == 0xeaa0d000) ||
	  (curr_inst == 0xeaa0d002))
	{
	  struct minimal_symbol *stubsym, *libsym;

	  stubsym = lookup_minimal_symbol_by_pc (loc);
	  if (stubsym == NULL)
	    {
	      warning ("Unable to find symbol for 0x%lx", loc);
	      return orig_pc == pc ? 0 : pc & ~0x3;
	    }

	  libsym = lookup_minimal_symbol (DEPRECATED_SYMBOL_NAME (stubsym), NULL, NULL);
	  if (libsym == NULL)
	    {
	      warning ("Unable to find library symbol for %s\n",
		       DEPRECATED_SYMBOL_NAME (stubsym));
	      return orig_pc == pc ? 0 : pc & ~0x3;
	    }

	  return SYMBOL_VALUE (libsym);
	}

      /* Does it look like bl X,%rp or bl X,%r0?  Another way to do a
         branch from the stub to the actual function.  */
      /*elz */
      else if ((curr_inst & 0xffe0e000) == 0xe8400000
	       || (curr_inst & 0xffe0e000) == 0xe8000000
	       || (curr_inst & 0xffe0e000) == 0xe800A000)
	return (loc + hppa_extract_17 (curr_inst) + 8) & ~0x3;

      /* Does it look like bv (rp)?   Note this depends on the
         current stack pointer being the same as the stack
         pointer in the stub itself!  This is a branch on from the
         stub back to the original caller.  */
      /*else if ((curr_inst & 0xffe0e000) == 0xe840c000) */
      else if ((curr_inst & 0xffe0f000) == 0xe840c000)
	{
	  /* Yup.  See if the previous instruction loaded
	     rp from sp - 8.  */
	  if (prev_inst == 0x4bc23ff1)
	    return (read_memory_integer
		    (read_register (HPPA_SP_REGNUM) - 8, 4)) & ~0x3;
	  else
	    {
	      warning ("Unable to find restore of %%rp before bv (%%rp).");
	      return orig_pc == pc ? 0 : pc & ~0x3;
	    }
	}

      /* elz: added this case to capture the new instruction
         at the end of the return part of an export stub used by
         the PA2.0: BVE, n (rp) */
      else if ((curr_inst & 0xffe0f000) == 0xe840d000)
	{
	  return (read_memory_integer
		  (read_register (HPPA_SP_REGNUM) - 24, TARGET_PTR_BIT / 8)) & ~0x3;
	}

      /* What about be,n 0(sr0,%rp)?  It's just another way we return to
         the original caller from the stub.  Used in dynamic executables.  */
      else if (curr_inst == 0xe0400002)
	{
	  /* The value we jump to is sitting in sp - 24.  But that's
	     loaded several instructions before the be instruction.
	     I guess we could check for the previous instruction being
	     mtsp %r1,%sr0 if we want to do sanity checking.  */
	  return (read_memory_integer
		  (read_register (HPPA_SP_REGNUM) - 24, TARGET_PTR_BIT / 8)) & ~0x3;
	}

      /* Haven't found the branch yet, but we're still in the stub.
         Keep looking.  */
      loc += 4;
    }
}


/* Exception handling support for the HP-UX ANSI C++ compiler.
   The compiler (aCC) provides a callback for exception events;
   GDB can set a breakpoint on this callback and find out what
   exception event has occurred. */

/* The name of the hook to be set to point to the callback function */
static char HP_ACC_EH_notify_hook[] = "__eh_notify_hook";
/* The name of the function to be used to set the hook value */
static char HP_ACC_EH_set_hook_value[] = "__eh_set_hook_value";
/* The name of the callback function in end.o */
static char HP_ACC_EH_notify_callback[] = "__d_eh_notify_callback";
/* Name of function in end.o on which a break is set (called by above) */
static char HP_ACC_EH_break[] = "__d_eh_break";
/* Name of flag (in end.o) that enables catching throws */
static char HP_ACC_EH_catch_throw[] = "__d_eh_catch_throw";
/* Name of flag (in end.o) that enables catching catching */
static char HP_ACC_EH_catch_catch[] = "__d_eh_catch_catch";
/* The enum used by aCC */
typedef enum
  {
    __EH_NOTIFY_THROW,
    __EH_NOTIFY_CATCH
  }
__eh_notification;

/* Is exception-handling support available with this executable? */
static int hp_cxx_exception_support = 0;
/* Has the initialize function been run? */
static int hp_cxx_exception_support_initialized = 0;
/* Address of __eh_notify_hook */
static CORE_ADDR eh_notify_hook_addr = 0;
/* Address of __d_eh_notify_callback */
static CORE_ADDR eh_notify_callback_addr = 0;
/* Address of __d_eh_break */
static CORE_ADDR eh_break_addr = 0;
/* Address of __d_eh_catch_catch */
static CORE_ADDR eh_catch_catch_addr = 0;
/* Address of __d_eh_catch_throw */
static CORE_ADDR eh_catch_throw_addr = 0;
/* Sal for __d_eh_break */
static struct symtab_and_line *break_callback_sal = 0;

/* Code in end.c expects __d_pid to be set in the inferior,
   otherwise __d_eh_notify_callback doesn't bother to call
   __d_eh_break!  So we poke the pid into this symbol
   ourselves.
   0 => success
   1 => failure  */
int
setup_d_pid_in_inferior (void)
{
  CORE_ADDR anaddr;
  struct minimal_symbol *msymbol;
  char buf[4];			/* FIXME 32x64? */

  /* Slam the pid of the process into __d_pid; failing is only a warning!  */
  msymbol = lookup_minimal_symbol ("__d_pid", NULL, symfile_objfile);
  if (msymbol == NULL)
    {
      warning ("Unable to find __d_pid symbol in object file.");
      warning ("Suggest linking executable with -g (links in /opt/langtools/lib/end.o).");
      return 1;
    }

  anaddr = SYMBOL_VALUE_ADDRESS (msymbol);
  store_unsigned_integer (buf, 4, PIDGET (inferior_ptid)); /* FIXME 32x64? */
  if (target_write_memory (anaddr, buf, 4))	/* FIXME 32x64? */
    {
      warning ("Unable to write __d_pid");
      warning ("Suggest linking executable with -g (links in /opt/langtools/lib/end.o).");
      return 1;
    }
  return 0;
}

/* elz: Used to lookup a symbol in the shared libraries.
   This function calls shl_findsym, indirectly through a
   call to __d_shl_get. __d_shl_get is in end.c, which is always
   linked in by the hp compilers/linkers. 
   The call to shl_findsym cannot be made directly because it needs
   to be active in target address space. 
   inputs: - minimal symbol pointer for the function we want to look up
   - address in target space of the descriptor for the library
   where we want to look the symbol up.
   This address is retrieved using the 
   som_solib_get_solib_by_pc function (somsolib.c). 
   output: - real address in the library of the function.          
   note: the handle can be null, in which case shl_findsym will look for
   the symbol in all the loaded shared libraries.
   files to look at if you need reference on this stuff:
   dld.c, dld_shl_findsym.c
   end.c
   man entry for shl_findsym */

CORE_ADDR
find_stub_with_shl_get (struct minimal_symbol *function, CORE_ADDR handle)
{
  struct symbol *get_sym, *symbol2;
  struct minimal_symbol *buff_minsym, *msymbol;
  struct type *ftype;
  struct value **args;
  struct value *funcval;
  struct value *val;

  int x, namelen, err_value, tmp = -1;
  CORE_ADDR endo_buff_addr, value_return_addr, errno_return_addr;
  CORE_ADDR stub_addr;


  args = alloca (sizeof (struct value *) * 8);		/* 6 for the arguments and one null one??? */
  funcval = find_function_in_inferior ("__d_shl_get");
  get_sym = lookup_symbol ("__d_shl_get", NULL, VAR_DOMAIN, NULL, NULL);
  buff_minsym = lookup_minimal_symbol ("__buffer", NULL, NULL);
  msymbol = lookup_minimal_symbol ("__shldp", NULL, NULL);
  symbol2 = lookup_symbol ("__shldp", NULL, VAR_DOMAIN, NULL, NULL);
  endo_buff_addr = SYMBOL_VALUE_ADDRESS (buff_minsym);
  namelen = strlen (DEPRECATED_SYMBOL_NAME (function));
  value_return_addr = endo_buff_addr + namelen;
  ftype = check_typedef (SYMBOL_TYPE (get_sym));

  /* do alignment */
  if ((x = value_return_addr % 64) != 0)
    value_return_addr = value_return_addr + 64 - x;

  errno_return_addr = value_return_addr + 64;


  /* set up stuff needed by __d_shl_get in buffer in end.o */

  target_write_memory (endo_buff_addr, DEPRECATED_SYMBOL_NAME (function), namelen);

  target_write_memory (value_return_addr, (char *) &tmp, 4);

  target_write_memory (errno_return_addr, (char *) &tmp, 4);

  target_write_memory (SYMBOL_VALUE_ADDRESS (msymbol),
		       (char *) &handle, 4);

  /* now prepare the arguments for the call */

  args[0] = value_from_longest (TYPE_FIELD_TYPE (ftype, 0), 12);
  args[1] = value_from_pointer (TYPE_FIELD_TYPE (ftype, 1), SYMBOL_VALUE_ADDRESS (msymbol));
  args[2] = value_from_pointer (TYPE_FIELD_TYPE (ftype, 2), endo_buff_addr);
  args[3] = value_from_longest (TYPE_FIELD_TYPE (ftype, 3), TYPE_PROCEDURE);
  args[4] = value_from_pointer (TYPE_FIELD_TYPE (ftype, 4), value_return_addr);
  args[5] = value_from_pointer (TYPE_FIELD_TYPE (ftype, 5), errno_return_addr);

  /* now call the function */

  val = call_function_by_hand (funcval, 6, args);

  /* now get the results */

  target_read_memory (errno_return_addr, (char *) &err_value, sizeof (err_value));

  target_read_memory (value_return_addr, (char *) &stub_addr, sizeof (stub_addr));
  if (stub_addr <= 0)
    error ("call to __d_shl_get failed, error code is %d", err_value);

  return (stub_addr);
}

/* Cover routine for find_stub_with_shl_get to pass to catch_errors */
static int
cover_find_stub_with_shl_get (void *args_untyped)
{
  args_for_find_stub *args = args_untyped;
  args->return_val = find_stub_with_shl_get (args->msym, args->solib_handle);
  return 0;
}

/* Initialize exception catchpoint support by looking for the
   necessary hooks/callbacks in end.o, etc., and set the hook value to
   point to the required debug function

   Return 0 => failure
   1 => success          */

static int
initialize_hp_cxx_exception_support (void)
{
  struct symtabs_and_lines sals;
  struct cleanup *old_chain;
  struct cleanup *canonical_strings_chain = NULL;
  int i;
  char *addr_start;
  char *addr_end = NULL;
  char **canonical = (char **) NULL;
  int thread = -1;
  struct symbol *sym = NULL;
  struct minimal_symbol *msym = NULL;
  struct objfile *objfile;
  asection *shlib_info;

  /* Detect and disallow recursion.  On HP-UX with aCC, infinite
     recursion is a possibility because finding the hook for exception
     callbacks involves making a call in the inferior, which means
     re-inserting breakpoints which can re-invoke this code */

  static int recurse = 0;
  if (recurse > 0)
    {
      hp_cxx_exception_support_initialized = 0;
      deprecated_exception_support_initialized = 0;
      return 0;
    }

  hp_cxx_exception_support = 0;

  /* First check if we have seen any HP compiled objects; if not,
     it is very unlikely that HP's idiosyncratic callback mechanism
     for exception handling debug support will be available!
     This will percolate back up to breakpoint.c, where our callers
     will decide to try the g++ exception-handling support instead. */
  if (!deprecated_hp_som_som_object_present)
    return 0;

  /* We have a SOM executable with SOM debug info; find the hooks */

  /* First look for the notify hook provided by aCC runtime libs */
  /* If we find this symbol, we conclude that the executable must
     have HP aCC exception support built in.  If this symbol is not
     found, even though we're a HP SOM-SOM file, we may have been
     built with some other compiler (not aCC).  This results percolates
     back up to our callers in breakpoint.c which can decide to
     try the g++ style of exception support instead.
     If this symbol is found but the other symbols we require are
     not found, there is something weird going on, and g++ support
     should *not* be tried as an alternative.

     ASSUMPTION: Only HP aCC code will have __eh_notify_hook defined.  
     ASSUMPTION: HP aCC and g++ modules cannot be linked together. */

  /* libCsup has this hook; it'll usually be non-debuggable */
  msym = lookup_minimal_symbol (HP_ACC_EH_notify_hook, NULL, NULL);
  if (msym)
    {
      eh_notify_hook_addr = SYMBOL_VALUE_ADDRESS (msym);
      hp_cxx_exception_support = 1;
    }
  else
    {
      warning ("Unable to find exception callback hook (%s).", HP_ACC_EH_notify_hook);
      warning ("Executable may not have been compiled debuggable with HP aCC.");
      warning ("GDB will be unable to intercept exception events.");
      eh_notify_hook_addr = 0;
      hp_cxx_exception_support = 0;
      return 0;
    }

  /* Next look for the notify callback routine in end.o */
  /* This is always available in the SOM symbol dictionary if end.o is linked in */
  msym = lookup_minimal_symbol (HP_ACC_EH_notify_callback, NULL, NULL);
  if (msym)
    {
      eh_notify_callback_addr = SYMBOL_VALUE_ADDRESS (msym);
      hp_cxx_exception_support = 1;
    }
  else
    {
      warning ("Unable to find exception callback routine (%s).", HP_ACC_EH_notify_callback);
      warning ("Suggest linking executable with -g (links in /opt/langtools/lib/end.o).");
      warning ("GDB will be unable to intercept exception events.");
      eh_notify_callback_addr = 0;
      return 0;
    }

#ifndef GDB_TARGET_IS_HPPA_20W
  /* Check whether the executable is dynamically linked or archive bound */
  /* With an archive-bound executable we can use the raw addresses we find
     for the callback function, etc. without modification. For an executable
     with shared libraries, we have to do more work to find the plabel, which
     can be the target of a call through $$dyncall from the aCC runtime support
     library (libCsup) which is linked shared by default by aCC. */
  /* This test below was copied from somsolib.c/somread.c.  It may not be a very
     reliable one to test that an executable is linked shared. pai/1997-07-18 */
  shlib_info = bfd_get_section_by_name (symfile_objfile->obfd, "$SHLIB_INFO$");
  if (shlib_info && (bfd_section_size (symfile_objfile->obfd, shlib_info) != 0))
    {
      /* The minsym we have has the local code address, but that's not the
         plabel that can be used by an inter-load-module call. */
      /* Find solib handle for main image (which has end.o), and use that
         and the min sym as arguments to __d_shl_get() (which does the equivalent
         of shl_findsym()) to find the plabel. */

      args_for_find_stub args;
      static char message[] = "Error while finding exception callback hook:\n";

      args.solib_handle = som_solib_get_solib_by_pc (eh_notify_callback_addr);
      args.msym = msym;
      args.return_val = 0;

      recurse++;
      catch_errors (cover_find_stub_with_shl_get, &args, message,
		    RETURN_MASK_ALL);
      eh_notify_callback_addr = args.return_val;
      recurse--;

      deprecated_exception_catchpoints_are_fragile = 1;

      if (!eh_notify_callback_addr)
	{
	  /* We can get here either if there is no plabel in the export list
	     for the main image, or if something strange happened (?) */
	  warning ("Couldn't find a plabel (indirect function label) for the exception callback.");
	  warning ("GDB will not be able to intercept exception events.");
	  return 0;
	}
    }
  else
    deprecated_exception_catchpoints_are_fragile = 0;
#endif

  /* Now, look for the breakpointable routine in end.o */
  /* This should also be available in the SOM symbol dict. if end.o linked in */
  msym = lookup_minimal_symbol (HP_ACC_EH_break, NULL, NULL);
  if (msym)
    {
      eh_break_addr = SYMBOL_VALUE_ADDRESS (msym);
      hp_cxx_exception_support = 1;
    }
  else
    {
      warning ("Unable to find exception callback routine to set breakpoint (%s).", HP_ACC_EH_break);
      warning ("Suggest linking executable with -g (link in /opt/langtools/lib/end.o).");
      warning ("GDB will be unable to intercept exception events.");
      eh_break_addr = 0;
      return 0;
    }

  /* Next look for the catch enable flag provided in end.o */
  sym = lookup_symbol (HP_ACC_EH_catch_catch, (struct block *) NULL,
		       VAR_DOMAIN, 0, (struct symtab **) NULL);
  if (sym)			/* sometimes present in debug info */
    {
      eh_catch_catch_addr = SYMBOL_VALUE_ADDRESS (sym);
      hp_cxx_exception_support = 1;
    }
  else
    /* otherwise look in SOM symbol dict. */
    {
      msym = lookup_minimal_symbol (HP_ACC_EH_catch_catch, NULL, NULL);
      if (msym)
	{
	  eh_catch_catch_addr = SYMBOL_VALUE_ADDRESS (msym);
	  hp_cxx_exception_support = 1;
	}
      else
	{
	  warning ("Unable to enable interception of exception catches.");
	  warning ("Executable may not have been compiled debuggable with HP aCC.");
	  warning ("Suggest linking executable with -g (link in /opt/langtools/lib/end.o).");
	  return 0;
	}
    }

  /* Next look for the catch enable flag provided end.o */
  sym = lookup_symbol (HP_ACC_EH_catch_catch, (struct block *) NULL,
		       VAR_DOMAIN, 0, (struct symtab **) NULL);
  if (sym)			/* sometimes present in debug info */
    {
      eh_catch_throw_addr = SYMBOL_VALUE_ADDRESS (sym);
      hp_cxx_exception_support = 1;
    }
  else
    /* otherwise look in SOM symbol dict. */
    {
      msym = lookup_minimal_symbol (HP_ACC_EH_catch_throw, NULL, NULL);
      if (msym)
	{
	  eh_catch_throw_addr = SYMBOL_VALUE_ADDRESS (msym);
	  hp_cxx_exception_support = 1;
	}
      else
	{
	  warning ("Unable to enable interception of exception throws.");
	  warning ("Executable may not have been compiled debuggable with HP aCC.");
	  warning ("Suggest linking executable with -g (link in /opt/langtools/lib/end.o).");
	  return 0;
	}
    }

  /* Set the flags */
  hp_cxx_exception_support = 2;	/* everything worked so far */
  hp_cxx_exception_support_initialized = 1;
  deprecated_exception_support_initialized = 1;

  return 1;
}

/* Target operation for enabling or disabling interception of
   exception events.
   KIND is either EX_EVENT_THROW or EX_EVENT_CATCH
   ENABLE is either 0 (disable) or 1 (enable).
   Return value is NULL if no support found;
   -1 if something went wrong,
   or a pointer to a symtab/line struct if the breakpointable
   address was found. */

struct symtab_and_line *
child_enable_exception_callback (enum exception_event_kind kind, int enable)
{
  char buf[4];

  if (!deprecated_exception_support_initialized
      || !hp_cxx_exception_support_initialized)
    if (!initialize_hp_cxx_exception_support ())
      return NULL;

  switch (hp_cxx_exception_support)
    {
    case 0:
      /* Assuming no HP support at all */
      return NULL;
    case 1:
      /* HP support should be present, but something went wrong */
      return (struct symtab_and_line *) -1;	/* yuck! */
      /* there may be other cases in the future */
    }

  /* Set the EH hook to point to the callback routine */
  store_unsigned_integer (buf, 4, enable ? eh_notify_callback_addr : 0);	/* FIXME 32x64 problem */
  /* pai: (temp) FIXME should there be a pack operation first? */
  if (target_write_memory (eh_notify_hook_addr, buf, 4))	/* FIXME 32x64 problem */
    {
      warning ("Could not write to target memory for exception event callback.");
      warning ("Interception of exception events may not work.");
      return (struct symtab_and_line *) -1;
    }
  if (enable)
    {
      /* Ensure that __d_pid is set up correctly -- end.c code checks this. :-( */
      if (PIDGET (inferior_ptid) > 0)
	{
	  if (setup_d_pid_in_inferior ())
	    return (struct symtab_and_line *) -1;
	}
      else
	{
	  warning ("Internal error: Invalid inferior pid?  Cannot intercept exception events.");
	  return (struct symtab_and_line *) -1;
	}
    }

  switch (kind)
    {
    case EX_EVENT_THROW:
      store_unsigned_integer (buf, 4, enable ? 1 : 0);
      if (target_write_memory (eh_catch_throw_addr, buf, 4))	/* FIXME 32x64? */
	{
	  warning ("Couldn't enable exception throw interception.");
	  return (struct symtab_and_line *) -1;
	}
      break;
    case EX_EVENT_CATCH:
      store_unsigned_integer (buf, 4, enable ? 1 : 0);
      if (target_write_memory (eh_catch_catch_addr, buf, 4))	/* FIXME 32x64? */
	{
	  warning ("Couldn't enable exception catch interception.");
	  return (struct symtab_and_line *) -1;
	}
      break;
    default:
      error ("Request to enable unknown or unsupported exception event.");
    }

  /* Copy break address into new sal struct, malloc'ing if needed. */
  if (!break_callback_sal)
    {
      break_callback_sal = (struct symtab_and_line *) xmalloc (sizeof (struct symtab_and_line));
    }
  init_sal (break_callback_sal);
  break_callback_sal->symtab = NULL;
  break_callback_sal->pc = eh_break_addr;
  break_callback_sal->line = 0;
  break_callback_sal->end = eh_break_addr;

  return break_callback_sal;
}

/* Record some information about the current exception event */
static struct exception_event_record current_ex_event;
/* Convenience struct */
static struct symtab_and_line null_symtab_and_line =
{NULL, 0, 0, 0};

/* Report current exception event.  Returns a pointer to a record
   that describes the kind of the event, where it was thrown from,
   and where it will be caught.  More information may be reported
   in the future */
struct exception_event_record *
child_get_current_exception_event (void)
{
  CORE_ADDR event_kind;
  CORE_ADDR throw_addr;
  CORE_ADDR catch_addr;
  struct frame_info *fi, *curr_frame;
  int level = 1;

  curr_frame = get_current_frame ();
  if (!curr_frame)
    return (struct exception_event_record *) NULL;

  /* Go up one frame to __d_eh_notify_callback, because at the
     point when this code is executed, there's garbage in the
     arguments of __d_eh_break. */
  fi = find_relative_frame (curr_frame, &level);
  if (level != 0)
    return (struct exception_event_record *) NULL;

  select_frame (fi);

  /* Read in the arguments */
  /* __d_eh_notify_callback() is called with 3 arguments:
     1. event kind catch or throw
     2. the target address if known
     3. a flag -- not sure what this is. pai/1997-07-17 */
  event_kind = read_register (HPPA_ARG0_REGNUM);
  catch_addr = read_register (HPPA_ARG1_REGNUM);

  /* Now go down to a user frame */
  /* For a throw, __d_eh_break is called by
     __d_eh_notify_callback which is called by
     __notify_throw which is called
     from user code.
     For a catch, __d_eh_break is called by
     __d_eh_notify_callback which is called by
     <stackwalking stuff> which is called by
     __throw__<stuff> or __rethrow_<stuff> which is called
     from user code. */
  /* FIXME: Don't use such magic numbers; search for the frames */
  level = (event_kind == EX_EVENT_THROW) ? 3 : 4;
  fi = find_relative_frame (curr_frame, &level);
  if (level != 0)
    return (struct exception_event_record *) NULL;

  select_frame (fi);
  throw_addr = get_frame_pc (fi);

  /* Go back to original (top) frame */
  select_frame (curr_frame);

  current_ex_event.kind = (enum exception_event_kind) event_kind;
  current_ex_event.throw_sal = find_pc_line (throw_addr, 1);
  current_ex_event.catch_sal = find_pc_line (catch_addr, 1);

  return &current_ex_event;
}

/* Signal frames.  */
struct hppa_hpux_sigtramp_unwind_cache
{
  CORE_ADDR base;
  struct trad_frame_saved_reg *saved_regs;
};

static int hppa_hpux_tramp_reg[] = {
  HPPA_SAR_REGNUM,
  HPPA_PCOQ_HEAD_REGNUM,
  HPPA_PCSQ_HEAD_REGNUM,
  HPPA_PCOQ_TAIL_REGNUM,
  HPPA_PCSQ_TAIL_REGNUM,
  HPPA_EIEM_REGNUM,
  HPPA_IIR_REGNUM,
  HPPA_ISR_REGNUM,
  HPPA_IOR_REGNUM,
  HPPA_IPSW_REGNUM,
  -1,
  HPPA_SR4_REGNUM,
  HPPA_SR4_REGNUM + 1,
  HPPA_SR4_REGNUM + 2,
  HPPA_SR4_REGNUM + 3,
  HPPA_SR4_REGNUM + 4,
  HPPA_SR4_REGNUM + 5,
  HPPA_SR4_REGNUM + 6,
  HPPA_SR4_REGNUM + 7,
  HPPA_RCR_REGNUM,
  HPPA_PID0_REGNUM,
  HPPA_PID1_REGNUM,
  HPPA_CCR_REGNUM,
  HPPA_PID2_REGNUM,
  HPPA_PID3_REGNUM,
  HPPA_TR0_REGNUM,
  HPPA_TR0_REGNUM + 1,
  HPPA_TR0_REGNUM + 2,
  HPPA_CR27_REGNUM
};

static struct hppa_hpux_sigtramp_unwind_cache *
hppa_hpux_sigtramp_frame_unwind_cache (struct frame_info *next_frame,
				       void **this_cache)

{
  struct gdbarch *gdbarch = get_frame_arch (next_frame);
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  struct hppa_hpux_sigtramp_unwind_cache *info;
  unsigned int flag;
  CORE_ADDR sp, scptr;
  int i, incr, off, szoff;

  if (*this_cache)
    return *this_cache;

  info = FRAME_OBSTACK_ZALLOC (struct hppa_hpux_sigtramp_unwind_cache);
  *this_cache = info;
  info->saved_regs = trad_frame_alloc_saved_regs (next_frame);

  sp = frame_unwind_register_unsigned (next_frame, HPPA_SP_REGNUM);

  scptr = sp - 1352;
  off = scptr;

  /* See /usr/include/machine/save_state.h for the structure of the save_state_t
     structure. */
  
  flag = read_memory_unsigned_integer(scptr, 4);
    
  if (!(flag & 0x40))
    {
      /* Narrow registers. */
      off = scptr + offsetof (save_state_t, ss_narrow);
      incr = 4;
      szoff = 0;
    }
  else
    {
      /* Wide registers. */
      off = scptr + offsetof (save_state_t, ss_wide) + 8;
      incr = 8;
      szoff = (tdep->bytes_per_address == 4 ? 4 : 0);
    }

  for (i = 1; i < 32; i++)
    {
      info->saved_regs[HPPA_R0_REGNUM + i].addr = off + szoff;
      off += incr;
    }

  for (i = 0; 
       i < sizeof(hppa_hpux_tramp_reg) / sizeof(hppa_hpux_tramp_reg[0]);
       i++)
    {
      if (hppa_hpux_tramp_reg[i] > 0)
        info->saved_regs[hppa_hpux_tramp_reg[i]].addr = off + szoff;
      off += incr;
    }

  /* TODO: fp regs */

  info->base = frame_unwind_register_unsigned (next_frame, HPPA_SP_REGNUM);

  return info;
}

static void
hppa_hpux_sigtramp_frame_this_id (struct frame_info *next_frame,
				   void **this_prologue_cache,
				   struct frame_id *this_id)
{
  struct hppa_hpux_sigtramp_unwind_cache *info
    = hppa_hpux_sigtramp_frame_unwind_cache (next_frame, this_prologue_cache);
  *this_id = frame_id_build (info->base, frame_pc_unwind (next_frame));
}

static void
hppa_hpux_sigtramp_frame_prev_register (struct frame_info *next_frame,
					 void **this_prologue_cache,
					 int regnum, int *optimizedp,
					 enum lval_type *lvalp, 
					 CORE_ADDR *addrp,
					 int *realnump, void *valuep)
{
  struct hppa_hpux_sigtramp_unwind_cache *info
    = hppa_hpux_sigtramp_frame_unwind_cache (next_frame, this_prologue_cache);
  hppa_frame_prev_register_helper (next_frame, info->saved_regs, regnum,
		                   optimizedp, lvalp, addrp, realnump, valuep);
}

static const struct frame_unwind hppa_hpux_sigtramp_frame_unwind = {
  SIGTRAMP_FRAME,
  hppa_hpux_sigtramp_frame_this_id,
  hppa_hpux_sigtramp_frame_prev_register
};

static const struct frame_unwind *
hppa_hpux_sigtramp_unwind_sniffer (struct frame_info *next_frame)
{
  CORE_ADDR pc = frame_pc_unwind (next_frame);
  char *name;

  find_pc_partial_function (pc, &name, NULL, NULL);

  if (name && strcmp(name, "_sigreturn") == 0)
    return &hppa_hpux_sigtramp_frame_unwind;

  return NULL;
}

static CORE_ADDR
hppa_hpux_som_find_global_pointer (struct value *function)
{
  CORE_ADDR faddr;
  
  faddr = value_as_address (function);

  /* Is this a plabel? If so, dereference it to get the gp value.  */
  if (faddr & 2)
    {
      int status;
      char buf[4];

      faddr &= ~3;

      status = target_read_memory (faddr + 4, buf, sizeof (buf));
      if (status == 0)
	return extract_unsigned_integer (buf, sizeof (buf));
    }

  return som_solib_get_got_by_pc (faddr);
}

static CORE_ADDR
hppa_hpux_push_dummy_code (struct gdbarch *gdbarch, CORE_ADDR sp,
			   CORE_ADDR funcaddr, int using_gcc,
			   struct value **args, int nargs,
			   struct type *value_type,
			   CORE_ADDR *real_pc, CORE_ADDR *bp_addr)
{
  /* FIXME: tausq/2004-06-09: This needs much more testing.  It is broken
     for pa64, but we should be able to get it to work with a little bit
     of work. gdb-6.1 has a lot of code to handle various cases; I've tried to
     simplify it and avoid compile-time conditionals.  */

  /* On HPUX, functions in the main executable and in libraries can be located
     in different spaces.  In order for us to be able to select the right 
     space for the function call, we need to go through an instruction seqeunce
     to select the right space for the target function, call it, and then
     restore the space on return.

     There are two helper routines that can be used for this task -- if
     an application is linked with gcc, it will contain a __gcc_plt_call
     helper function.  __gcc_plt_call, when passed the entry point of an
     import stub, will do the necessary space setting/restoration for the
     target function.

     For programs that are compiled/linked with the HP compiler, a similar
     function called __d_plt_call exists; __d_plt_call expects a PLABEL instead
     of an import stub as an argument.

     // *INDENT-OFF*
     To summarize, the call flow is:
       current function -> dummy frame -> __gcc_plt_call (import stub) 
                        -> target function
     or
       current function -> dummy frame -> __d_plt_call (plabel)
                        -> target function
     // *INDENT-ON*

     In general the "funcaddr" argument passed to push_dummy_code is the actual
     entry point of the target function.  For __gcc_plt_call, we need to 
     locate the import stub for the corresponding function.  Failing that,
     we construct a dummy "import stub" on the stack to pass as an argument.
     For __d_plt_call, we similarly synthesize a PLABEL on the stack to
     pass to the helper function.

     An additional twist is that, in order for us to restore the space register
     to its starting state, we need __gcc_plt_call/__d_plt_call to return
     to the instruction where we started the call.  However, if we put
     the breakpoint there, gdb will complain because it will find two 
     frames on the stack with the same (sp, pc) (with the dummy frame in 
     between).  Currently, we set the return pointer to (pc - 4) of the 
     current function.  FIXME: This is not an ideal solution; possibly if the 
     current pc is at the beginning of a page, this will cause a page fault. 
     Need to understand this better and figure out a better way to fix it.  */

  struct minimal_symbol *sym;

  /* Nonzero if we will use GCC's PLT call routine.  This routine must be
     passed an import stub, not a PLABEL.  It is also necessary to get %r19
     before performing the call.  (This is done by push_dummy_call.)  */
  int use_gcc_plt_call = 1;

  /* See if __gcc_plt_call is available; if not we will use the HP version
     instead.  */
  sym = lookup_minimal_symbol ("__gcc_plt_call", NULL, NULL);
  if (sym == NULL)
    use_gcc_plt_call = 0;

  /* If using __gcc_plt_call, we need to make sure we pass in an import
     stub.  funcaddr can be pointing to an export stub or a real function,
     so we try to resolve it to the import stub.  */
  if (use_gcc_plt_call)
    {
      struct objfile *objfile;
      struct minimal_symbol *funsym, *stubsym;
      CORE_ADDR stubaddr = 0;

      funsym = lookup_minimal_symbol_by_pc (funcaddr);
      if (!funsym)
        error ("Unable to find symbol for target function.\n");

      ALL_OBJFILES (objfile)
        {
	  stubsym = lookup_minimal_symbol_solib_trampoline
	    (SYMBOL_LINKAGE_NAME (funsym), objfile);

          if (stubsym)
	    {
	      struct unwind_table_entry *u;

	      u = find_unwind_entry (SYMBOL_VALUE (stubsym));
	      if (u == NULL 
	          || (u->stub_unwind.stub_type != IMPORT
		      && u->stub_unwind.stub_type != IMPORT_SHLIB))
	        continue;

              stubaddr = SYMBOL_VALUE (stubsym);

	      /* If we found an IMPORT stub, then we can stop searching;
	         if we found an IMPORT_SHLIB, we want to continue the search
		 in the hopes that we will find an IMPORT stub.  */
	      if (u->stub_unwind.stub_type == IMPORT)
	        break;
	    }
	}

      if (stubaddr != 0)
        {
          /* Argument to __gcc_plt_call is passed in r22.  */
          regcache_cooked_write_unsigned (current_regcache, 22, stubaddr);
        }
      else
        {
	  /* No import stub found; let's synthesize one.  */

	  /* ldsid %r21, %r1 */
	  write_memory_unsigned_integer (sp, 4, 0x02a010a1);
	  /* mtsp %r1,%sr0 */
	  write_memory_unsigned_integer (sp + 4, 4, 0x00011820);
	  /* be 0(%sr0, %r21) */
	  write_memory_unsigned_integer (sp + 8, 4, 0xe2a00000);
          /* nop */
          write_memory_unsigned_integer (sp + 12, 4, 0x08000240);

          regcache_cooked_write_unsigned (current_regcache, 21, funcaddr);
          regcache_cooked_write_unsigned (current_regcache, 22, sp);
	}

      /* We set the breakpoint address and r31 to (close to) where the current
         pc is; when __gcc_plt_call returns, it will restore pcsqh to the
	 current value based on this.  The -4 is needed for frame unwinding
	 to work properly -- we need to land in a different function than
	 the current function.  */
      *bp_addr = (read_register (HPPA_PCOQ_HEAD_REGNUM) & ~3) - 4;
      regcache_cooked_write_unsigned (current_regcache, 31, *bp_addr);

      /* Continue from __gcc_plt_call.  */
      *real_pc = SYMBOL_VALUE (sym);
    }
  else
    {
      unsigned int gp;

      /* Use __d_plt_call as a fallback; __d_plt_call expects to be called 
         with a plabel, so we need to build one.  */

      sym = lookup_minimal_symbol ("__d_plt_call", NULL, NULL);
      if (sym == NULL)
        error("Can't find an address for __d_plt_call or __gcc_plt_call "
	      "trampoline\nSuggest linking executable with -g or compiling "
	      "with gcc.");

      gp = gdbarch_tdep (gdbarch)->find_global_pointer (funcaddr);
      write_memory_unsigned_integer (sp, 4, funcaddr);
      write_memory_unsigned_integer (sp + 4, 4, gp);

      /* plabel is passed in r22 */
      regcache_cooked_write_unsigned (current_regcache, 22, sp);
    }

  /* Pushed one stack frame, which has to be 64-byte aligned.  */
  sp += 64;

  return sp;
}

static void
hppa_hpux_inferior_created (struct target_ops *objfile, int from_tty)
{
  /* Some HP-UX related globals to clear when a new "main"
     symbol file is loaded.  HP-specific.  */
  deprecated_hp_som_som_object_present = 0;
  hp_cxx_exception_support_initialized = 0;
}

static void
hppa_hpux_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  if (tdep->bytes_per_address == 4)
    set_gdbarch_in_solib_call_trampoline (gdbarch, 
					  hppa32_hpux_in_solib_call_trampoline);
  else
    set_gdbarch_in_solib_call_trampoline (gdbarch, 
					  hppa64_hpux_in_solib_call_trampoline);

  set_gdbarch_in_solib_return_trampoline (gdbarch,
					  hppa_hpux_in_solib_return_trampoline);
  set_gdbarch_skip_trampoline_code (gdbarch, hppa_hpux_skip_trampoline_code);

  set_gdbarch_push_dummy_code (gdbarch, hppa_hpux_push_dummy_code);
  set_gdbarch_call_dummy_location (gdbarch, ON_STACK);

  frame_unwind_append_sniffer (gdbarch, hppa_hpux_sigtramp_unwind_sniffer);

  observer_attach_inferior_created (hppa_hpux_inferior_created);
}

static void
hppa_hpux_som_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  tdep->is_elf = 0;

  tdep->find_global_pointer = hppa_hpux_som_find_global_pointer;
  hppa_hpux_init_abi (info, gdbarch);
}

static void
hppa_hpux_elf_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  tdep->is_elf = 1;
  hppa_hpux_init_abi (info, gdbarch);
}

void
_initialize_hppa_hpux_tdep (void)
{
  gdbarch_register_osabi (bfd_arch_hppa, 0, GDB_OSABI_HPUX_SOM,
                          hppa_hpux_som_init_abi);
  gdbarch_register_osabi (bfd_arch_hppa, bfd_mach_hppa20w, GDB_OSABI_HPUX_ELF,
                          hppa_hpux_elf_init_abi);
}
