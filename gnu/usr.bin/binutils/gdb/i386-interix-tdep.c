/* Target-dependent code for Interix running on i386's, for GDB.
   Copyright 2002 Free Software Foundation, Inc.

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

#include "frame.h"
#include "gdb_string.h"
#include "gdb-stabs.h"
#include "gdbcore.h"
#include "gdbtypes.h"
#include "i386-tdep.h"
#include "inferior.h"
#include "libbfd.h"
#include "objfiles.h"
#include "osabi.h"
#include "regcache.h"

/* offsetof (mcontext_t, gregs.gregs[EBP]) */
static const int mcontext_EBP_greg_offset = 180;

/* offsetof (mcontext_t, gregs.gregs[EIP]) */
static const int mcontext_EIP_greg_offset = 184;

/* offsetof (mcontext_t, gregs.gregs[UESP]) */
static const int mcontext_UESP_greg_offset = 196;

/* offsetof (mcontext_t, gregs.reserved[1]) */
static const int mcontext_syscall_greg_offset = 4;

/* offsetof (_JUMP_BUFFER, Eip) */
static const int jump_buffer_Eip_offset = 20;

/* See procfs.c and *interix*.h in config/[alpha,i386].  */
/* ??? These should be static, but this needs a bit of work before this
   can be done.  */
CORE_ADDR tramp_start;
CORE_ADDR tramp_end;
CORE_ADDR null_start;
CORE_ADDR null_end;
int winver;                     /* Windows NT version number */

/* Forward declarations.  */
extern void _initialize_i386_interix_tdep (void);
extern initialize_file_ftype _initialize_i386_interix_tdep;

/* Adjust the section offsets in an objfile structure so that it's correct
   for the type of symbols being read (or undo it with the _restore
   arguments).  

   If main programs ever start showing up at other than the default Image
   Base, this is where that would likely be applied.  */

void
pei_adjust_objfile_offsets (struct objfile *objfile,
                            enum objfile_adjusts type)
{
  int i;
  CORE_ADDR symbols_offset;

  switch (type)
    {
    case adjust_for_symtab:
      symbols_offset = NONZERO_LINK_BASE (objfile->obfd);
      break;
    case adjust_for_symtab_restore:
      symbols_offset = -NONZERO_LINK_BASE (objfile->obfd);
      break;
    case adjust_for_stabs:
    case adjust_for_stabs_restore:
    case adjust_for_dwarf:
    case adjust_for_dwarf_restore:
    default:
      return;
    }

  for (i = 0; i < objfile->num_sections; i++)
    {
      (objfile->section_offsets)->offsets[i] += symbols_offset;
    }
}

static int
i386_interix_pc_in_sigtramp (CORE_ADDR pc, char *name)
{
  /* This is sufficient, where used, but is NOT a complete test; There
     is more in DEPRECATED_INIT_EXTRA_FRAME_INFO
     (a.k.a. interix_back_one_frame).  */
  return ((pc >= tramp_start && pc < tramp_end)
          || (pc >= null_start && pc < null_end));
}

static int
i386_interix_in_solib_call_trampoline (CORE_ADDR pc, char *name)
{
  return i386_pe_skip_trampoline_code (pc, name);
}

static CORE_ADDR
i386_interix_skip_trampoline_code (CORE_ADDR pc)
{
  return i386_pe_skip_trampoline_code (pc, 0);
}

static int
i386_interix_frame_chain_valid (CORE_ADDR chain, struct frame_info *thisframe)
{
  /* In the context where this is used, we get the saved PC before we've
     successfully unwound far enough to be sure what we've got (it may
     be a signal handler caller).  If we're dealing with a signal
     handler caller, this will return valid, which is fine.  If not,
     it'll make the correct test.  */
  return ((get_frame_type (thisframe) == SIGTRAMP_FRAME)
          || (chain != 0
              && !deprecated_inside_entry_file (read_memory_integer
						(thisframe->frame + 4, 4))));
}

/* We want to find the previous frame, which on Interix is tricky when
   signals are involved; set frame->frame appropriately, and also get
   the pc and tweak tye frame's type; this replaces a boatload of
   nested macros, as well.  */
static void
i386_interix_back_one_frame (int fromleaf, struct frame_info *frame)
{
  CORE_ADDR ra;
  CORE_ADDR fm;
  CORE_ADDR context;
  long t;

  if (frame == NULL)
    internal_error (__FILE__, __LINE__, "unexpected NULL frame");

  if (fromleaf)
    {
      frame->pc = DEPRECATED_SAVED_PC_AFTER_CALL (frame->next);
      return;
    }

  if (!frame->next)
    {
      frame->pc = read_pc ();

      /* Part of the signal stuff...  See below.  */
      if (stopped_by_random_signal)
        {
          /* We know we're in a system call mini-frame; was it
             NullApi or something else?  */
          ra = DEPRECATED_SAVED_PC_AFTER_CALL (frame);
          if (ra >= null_start && ra < null_end)
	    deprecated_set_frame_type (frame, SIGTRAMP_FRAME);
          /* There might also be an indirect call to the mini-frame,
             putting one more return address on the stack.  (XP only,
             I think?)  This can't (reasonably) return the address of the 
             signal handler caller unless it's that situation, so this
             is safe.  */
          ra = read_memory_unsigned_integer (read_register (SP_REGNUM) + 4, 4);
          if (ra >= null_start && ra < null_end)
	    deprecated_set_frame_type (frame, SIGTRAMP_FRAME);
        }
      return;
    }

  if (!(get_frame_type (frame->next) == SIGTRAMP_FRAME))
    {
      frame->pc = read_memory_integer (frame->next->frame + 4, 4);
      return;
    }

  /* This is messy (actually AWFUL)...  The "trampoline" might be 2, 3 
     or all 5 entities on the frame. 

     Chunk 1 will be present when we're actually in a signal handler.
     Chunk 2 will be present when an asynchronous signal (one that
     didn't come in with a system call) is present.
     We may not (yet) be in the handler, if we're just returning
     from the call.
     When we're actually in a handler taken from an asynchronous
     signal, both will be present.

     Chunk 1:
     PdxSignalDeliverer's frame 
     + Context struct    -- not accounted for in any frame

     Chunk 2:
     + PdxNullPosixApi's frame 
     + PdxNullApiCaller's frame
     + Context struct = 0x230  not accounted for in any frame

     The symbol names come from examining objdumps of psxdll.dll;
     they don't appear in the runtime image.

     For gdb's purposes, we can pile all this into one frame.  */

  ra = frame->next->pc;
  /* Are we already pointing at PdxNullPosixApi?  We are if
     this is a signal frame, we're at next-to-top, and were stopped
     by a random signal (if it wasn't the right address under
     these circumstances, we wouldn't be here at all by tests above
     on the prior frame).  */
  if (frame->next->next == NULL && stopped_by_random_signal)
    {
      /* We're pointing at the frame FOR PdxNullApi.  */
      fm = frame->frame;
    }
  else
    {
      /* No...  We must be pointing at the frame that was called
         by PdxSignalDeliverer; back up across the whole mess.  */

      /* Extract the frame for PdxSignalDeliverer.  Note:
         DEPRECATED_FRAME_CHAIN used the "old" frame pointer because
         we were a deliverer.  Get the address of the context record
         that's on here frameless.  */
      context = read_memory_integer (frame->frame, 4);  /* an Arg */

      /* Now extract the frame pointer contained in the context.  */
      fm = read_memory_integer (context + mcontext_EBP_greg_offset, 4);

      ra = read_memory_integer (context + mcontext_EIP_greg_offset, 4);

      /* We need to know if we're in a system call because we'll be
         in a syscall mini-frame, if so, and the rules are different.  */
      t = (long) read_memory_integer (context + mcontext_syscall_greg_offset,
                                      4);
      /* t contains 0 if running free, 1 if blocked on a system call,
         and 2 if blocked on an exception message (e.g. a trap);
         we don't expect to get here with a 2.  */
      if (t != 1)
        {
          /* Not at a system call, therefore it can't be NullApi.  */
          frame->pc = ra;
          frame->frame = fm;
          return;
        }

      /* It's a system call...  Mini frame, then look for NullApi.  */
      /* Get the RA (on the stack) associated with this...  It's
         a system call mini-frame.  */
      ra = read_memory_integer (context + mcontext_UESP_greg_offset, 4);

      if (winver >= 51)
        {
          /* Newer versions of Windows NT interpose another return
             address (but no other "stack frame" stuff) that we need
             to simply ignore here.  */
          ra += 4;
        }

      ra = read_memory_integer (ra, 4);

      if (!(ra >= null_start && ra < null_end))
        {
          /* No Null API present; we're done.  */
          frame->pc = ra;
          frame->frame = fm;
          return;
        }
    }

  /* At this point, we're looking at the frame for PdxNullPosixApi,
     in either case.

     PdxNullPosixApi is called by PdxNullApiCaller (which in turn
     is called by _PdxNullApiCaller (note the _).)
     PdxNullPosixApiCaller (no _) is a frameless function.

     The saved frame pointer is as fm, but it's not of interest
     to us because it skips us over the saved context, which is
     the wrong thing to do, because it skips the interrrupted
     routine!  PdxNullApiCaller takes as its only argument the
     address of the context of the interrupded function (which
     is really in no frame, but jammed on the stack by the system)

     So: fm+0: saved bp
     fm+4: return address to _PdxNullApiCaller
     fm+8: arg to PdxNullApiCaller pushed by _Pdx...  */

  fm = read_memory_integer (fm + 0x8, 4);

  /* Extract the second context record.  */

  ra = read_memory_integer (fm + mcontext_EIP_greg_offset, 4);
  fm = read_memory_integer (fm + mcontext_EBP_greg_offset, 4);

  frame->frame = fm;
  frame->pc = ra;

  return;
}

static CORE_ADDR
i386_interix_frame_saved_pc (struct frame_info *fi)
{
  /* Assume that we've already unwound enough to have the caller's address
     if we're dealing with a signal handler caller (And if that fails,
     return 0).  */
  if ((get_frame_type (fi) == SIGTRAMP_FRAME))
    return fi->next ? fi->next->pc : 0;
  else
    return read_memory_integer (fi->frame + 4, 4);
}

static void
i386_interix_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  tdep->struct_return = reg_struct_return;
  tdep->jb_pc_offset = jump_buffer_Eip_offset;

  set_gdbarch_pc_in_sigtramp (gdbarch, i386_interix_pc_in_sigtramp);
  set_gdbarch_in_solib_call_trampoline (gdbarch,
                                        i386_interix_in_solib_call_trampoline);
  set_gdbarch_skip_trampoline_code (gdbarch,
                                    i386_interix_skip_trampoline_code);
  set_gdbarch_deprecated_init_extra_frame_info (gdbarch, i386_interix_back_one_frame);
  set_gdbarch_deprecated_frame_chain_valid (gdbarch, i386_interix_frame_chain_valid);
  set_gdbarch_deprecated_frame_saved_pc (gdbarch, i386_interix_frame_saved_pc);
  set_gdbarch_name_of_malloc (gdbarch, "_malloc");
}

static enum gdb_osabi
i386_interix_osabi_sniffer (bfd * abfd)
{
  char *target_name = bfd_get_target (abfd);

  if (strcmp (target_name, "pei-i386") == 0)
    return GDB_OSABI_INTERIX;

  return GDB_OSABI_UNKNOWN;
}

void
_initialize_i386_interix_tdep (void)
{
  gdbarch_register_osabi_sniffer (bfd_arch_i386, bfd_target_coff_flavour,
                                  i386_interix_osabi_sniffer);

  gdbarch_register_osabi (bfd_arch_i386, 0, GDB_OSABI_INTERIX,
                          i386_interix_init_abi);
}
