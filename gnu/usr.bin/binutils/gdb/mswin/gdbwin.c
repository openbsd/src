/* C clue routines to talk to gdb */
#include <stdio.h>
#include <stdarg.h>
#include "defs.h"
#include "gdbwin.h"
#include "symtab.h"
#include "inferior.h"
#include "command.h"
#include "breakpoint.h"
#include "bfd.h"
#include "symfile.h"
#include "objfiles.h"
#include <setjmp.h>
#include "top.h"
#include <malloc.h>
#include <string.h>

#include "../include/dis-asm.h"
#include <fcntl.h>
#include "log.h" 
/* Hooks into gdb from gui */

const char *doing_something (const char *);

static int top_level_val;

/* Do a setjmp on error_return and quit_return.  catch_errors is
   generally a cleaner way to do this, but main() would look pretty
   ugly if it had to use catch_errors each time.  */

#define SET_TOP_LEVEL() \
  (((top_level_val = setjmp (error_return)) \
    ? (PTR) 0 : (PTR) memcpy (quit_return, error_return, sizeof (jmp_buf))) \
   , top_level_val)

void gdbwin_update (int regs, int pc, int bpt);
void gdbwin_fputs (const char *linebuffer, FILE *stream);

int regs_changed = 0;
int pc_changed = 0;
int bpt_changed = 0;
static void 
regs_changed_f (void)
{
  regs_changed = 1;
  pc_changed = 1;
}

static void 
bpt_changed_f (struct breakpoint *bpt)
{
  bpt_changed = 1;
}
/* Call ths after every entry to ensure that gui state
   is ok with gdb state */
static int
update (void *foo)
{
  int a = regs_changed;
  int b = pc_changed;
  int c = bpt_changed;

  regs_changed = 0;
  pc_changed = 0;
  bpt_changed = 0;
  gdbwin_update (a, b, c);

  return 1;
}

void 
togdb_force_update ()
{
  bpt_changed_f (NULL);
  regs_changed_f ();
  update (NULL);
}

static int 
run_execute_command (void *arg)
{
  extern void execute_command (char *p, int from_tty);
  char *p;

  p = arg;

  execute_command (p, 1);

  return 1;
}

void 
togdb_command_from_tty (const char *cmd)
{
  char *p;

  p = alloca (strlen (cmd) + 1);
  strcpy (p, cmd);

  catch_errors (run_execute_command, p, NULL, RETURN_MASK_ERROR);
  catch_errors (update, NULL, NULL, RETURN_MASK_ERROR);
}

void 
togdb_command (const char *cmd)
{
  char *p;

  p = alloca (strlen (cmd) + 1);
  strcpy (p, cmd);

  catch_errors (run_execute_command, p, NULL, RETURN_MASK_ERROR);
  catch_errors (update, NULL, NULL, RETURN_MASK_ERROR);
}

/* Fetch registers and return them in host order */
t_reg
togdb_fetchreg (int i)
{
  char raw_buffer[MAX_REGISTER_RAW_SIZE];

  read_relative_register_raw_bytes (i, raw_buffer);
  return extract_unsigned_integer (raw_buffer, REGISTER_RAW_SIZE (i));
}

/* Build a possible breakpoint table */
#if 0
struct lineinfo *
togdb_lineinfo (const char *filename)
{
  struct symtab *p = lookup_symtab (filename);

  return togdb_symtab_to_lineinfo (p);
}
#endif

void 
togdb_free_lineinfo (struct lineinfo *p)
{
  free (p);
}
/* Find where we are in the source world */

void 
togdb_fetchloc (CORE_ADDR pc,
		char **filename,
		int *line,
		struct symtab **symt)
{
  struct symtab_and_line sal;

  if (pc == 0)
    pc = togdb_fetchreg (PC_REGNUM);

  sal = find_pc_line (pc, 0);

  if (symt)
    *symt = sal.symtab;

  if (sal.symtab
      && sal.symtab->filename)
    {
      *filename = sal.symtab->filename;
    }
  else
    {
      *filename = "";
    }

  *line = sal.line;
}

int 
togdb_find_pc_partial_function (CORE_ADDR pc,
				char **name,
				CORE_ADDR * low,
				CORE_ADDR * high)
{
  return find_pc_partial_function (pc, name, low, high);
}

struct block *
togdb_get_frame_block (struct frame_info *p)
{
  return get_frame_block (p);
}

int 
togdb_target_has_execution ()
{
  return target_has_execution;
}

extern struct breakpoint *breakpoint_chain;

void *
togdb_breakinfo_i_init ()
{
  struct breakpoint *b = breakpoint_chain;

  while (b && b->address == 0)
    b = b->next;
  return (void *) b;
}
void *
togdb_breakinfo_i_next (void *p)
{
  struct breakpoint *b = (struct breakpoint *) p;

  /* Skip over uninteresting ones */
#if 1
  while (b && b->address == 0)
    b = b->next;
#endif
  if (b)
    return (void *) (b->next);
  return 0;
}

/* Accessores o turn the gdb breakpoint struct into something
   C++ can put in a class (see gdbwinxx.ccp) */

#define C(x) ((struct breakpoint *)x)
#if 0
int 
bi_number (void *p)
{
  return C (p)->number;
}
int 
bi_hitcount (void *p)
{
  return C (p)->hit_count;
}
int 
bi_type (void *p)
{
  return C (p)->type;
}
int 
bi_disposition (void *p)
{
  return C (p)->disposition;
}
int 
bi_enable (void *p)
{
  return C (p)->enable;
}
CORE_ADDR 
bi_address (void *p)
{
  return C (p)->address;
}
int 
bi_linenumber (void *p)
{
  return C (p)->line_number;
}
const char *
bi_sourcefile (void *p)
{
  return C (p)->source_file;
}
const char *
bi_condstring (void *p)
{
  return C (p)->cond_string;
}
const char *
bi_expstring (void *p)
{
  return C (p)->exp_string;
}
const char *
bi_addrstring (void *p)
{
  return C (p)->addr_string;
}
#endif

void 
bi_disable_bpt (void *p)
{
  disable_breakpoint (C (p));
  bpt_changed = 1;
  update (NULL);
}

void 
bi_enable_bpt (void *p)
{
  enable_breakpoint (C (p));
  bpt_changed = 1;
  update (NULL);
}

void 
bi_delete_all ()
{
  bpt_changed = 1;
  while (breakpoint_chain)
    delete_breakpoint (breakpoint_chain);
  update (NULL);
}

void 
bi_delete_breakpoint (void *p)
{
  bpt_changed = 1;
  delete_breakpoint (p);
  update (NULL);
}

void 
togdb_bpt_set (c)
     const char *c;
{
  char buf[200];

  bpt_changed = 1;

  sprintf (buf, "b %s", c);
  togdb_command (buf);
}

struct sas
{
  char *end;
};

static void
my_fputs_unfiltered (const char *data, FILE * fakestream)
{
  struct sas *stream = (struct sas *) fakestream;

  strcat (stream->end, data);
}

static void 
my_target_output_hook (char *s)
{
  CIOLogView_output (s);
}
int
togdb_disassemble (CORE_ADDR addr, char *buf)
{
  disassemble_info info;
  struct sas as;
  void (*old_fputs_unfiltered_hook) (const char *data, FILE * fakestream);
  int val;

  as.end = buf;
  *as.end = '\000';

  INIT_DISASSEMBLE_INFO (info, (FILE *) & as,
			 (fprintf_ftype) fprintf_unfiltered);
  info.read_memory_func = dis_asm_read_memory;
  info.memory_error_func = dis_asm_memory_error;
  info.print_address_func = dis_asm_print_address;
  info.insn_info_valid = 0;

  old_fputs_unfiltered_hook = fputs_unfiltered_hook;
  fputs_unfiltered_hook = my_fputs_unfiltered;

  if (!SET_TOP_LEVEL ())
    {
      print_address (addr, (FILE *) & as);
      fputs_unfiltered (":\t    ", (FILE *) & as);
      val = tm_print_insn (addr, &info);
    }
  else
    val = 0;

  fputs_unfiltered_hook = old_fputs_unfiltered_hook;

  return val;
}

CORE_ADDR 
togdb_fetchpc ()
{
  return selected_frame ? selected_frame->pc : stop_pc;
}

static void
gui_command (char *args, int from_tty)
{
  gdbwin_command (args);
}

extern int mswin_query (const char *, va_list);

void 
_initialize_gdbwin ()
{
  add_com ("gui", no_class, gui_command, "Enter a gui command");
  fputs_unfiltered_hook = gdbwin_fputs;
  registers_changed_hook = regs_changed_f;

  create_breakpoint_hook = bpt_changed_f;
  delete_breakpoint_hook = bpt_changed_f;
  modify_breakpoint_hook = bpt_changed_f;
  target_output_hook = my_target_output_hook;
  query_hook = mswin_query;
/*  progress_hook = doing_something; */
}

#if 0
/* struct frame_info accessors */

CORE_ADDR 
togdb_frameinfo_frameaddr (struct frame_info *f)
{
  return f->frame;
}

CORE_ADDR 
togdb_frameinfo_framepc (struct frame_info * f)
{
  return f->pc;
}
struct frame_info *
togdb_frameinfo_prevframe (struct frame_info *f)
{
  return get_prev_frame (f);
}

struct frame_info *
togdb_frameinfo_getcurrentframe ()
{
  return get_current_frame ();
}
#endif

/**********************************************************************/
/* General code to annotate frame_info structure - which can
   onedat be used by an ASCII or GUI interface */

void 
togdb_annotate_info (fi, fai)
     struct frame_info *fi;
     struct frame_annotated_info *fai;
{
  struct symtab_and_line sal;
  struct symbol *func;
  register char *funname = 0;
  enum language funlang = language_unknown;

  fai->funcname = 0;
  fai->filename = 0;
  fai->line = 0;

#if 0
  char buf[MAX_REGISTER_RAW_SIZE];
  CORE_ADDR sp;

  /* On the 68k, this spends too much time in m68k_find_saved_regs.  */

  /* Get the value of SP_REGNUM relative to the frame.  */
  get_saved_register (buf, (int *) NULL, (CORE_ADDR *) NULL,
		    FRAME_INFO_ID (fi), SP_REGNUM, (enum lval_type *) NULL);
  sp = extract_address (buf, REGISTER_RAW_SIZE (SP_REGNUM));

  /* This is not a perfect test, because if a function alloca's some
     memory, puts some code there, and then jumps into it, then the test
     will succeed even though there is no call dummy.  Probably best is
     to check for a bp_call_dummy breakpoint.  */
  if (PC_IN_CALL_DUMMY (fi->pc, sp, fi->frame))
#else
  if (frame_in_dummy (fi))
#endif
    {
      fai->called_by = CALLED_BY_GDB;
      return;
    }
  if (fi->signal_handler_caller)
    {
      fai->called_by = CALLED_BY_SIGNAL;
      return;
    }

  /* If fi is not the innermost frame, that normally means that fi->pc
     points to *after* the call instruction, and we want to get the line
     containing the call, never the next line.  But if the next frame is
     a signal_handler_caller or a dummy frame, then the next frame was
     not entered as the result of a call, and we want to get the line
     containing fi->pc.  */
  sal =
    find_pc_line (fi->pc,
		  fi->next != NULL
		  && !fi->next->signal_handler_caller
		  && !frame_in_dummy (fi->next));

  func = find_pc_function (fi->pc);
  if (func)
    {
      /* In certain pathological cases, the symtabs give the wrong
         function (when we are in the first function in a file which
         is compiled without debugging symbols, the previous function
         is compiled with debugging symbols, and the "foo.o" symbol
         that is supposed to tell us where the file with debugging symbols
         ends has been truncated by ar because it is longer than 15
         characters).  This also occurs if the user uses asm() to create
         a function but not stabs for it (in a file compiled -g).

         So look in the minimal symbol tables as well, and if it comes
         up with a larger address for the function use that instead.
         I don't think this can ever cause any problems; there shouldn't
         be any minimal symbols in the middle of a function; if this is
         ever changed many parts of GDB will need to be changed (and we'll
         create a find_pc_minimal_function or some such).  */

      struct minimal_symbol *msymbol = lookup_minimal_symbol_by_pc (fi->pc);

      if (msymbol != NULL
	  && (SYMBOL_VALUE_ADDRESS (msymbol)
	      > BLOCK_START (SYMBOL_BLOCK_VALUE (func))))
	{
#if 0
	  /* There is no particular reason to think the line number
	     information is wrong.  Someone might have just put in
	     a label with asm() but left the line numbers alone.  */
	  /* In this case we have no way of knowing the source file
	     and line number, so don't print them.  */
	  sal.symtab = 0;
#endif
	  /* We also don't know anything about the function besides
	     its address and name.  */
	  func = 0;
	  funname = SYMBOL_NAME (msymbol);
	  funlang = SYMBOL_LANGUAGE (msymbol);
	}
      else
	{
	  funname = SYMBOL_NAME (func);
	  funlang = SYMBOL_LANGUAGE (func);
	}
    }
  else
    {
      register struct minimal_symbol *msymbol = lookup_minimal_symbol_by_pc (fi->pc);

      if (msymbol != NULL)
	{
	  funname = SYMBOL_NAME (msymbol);
	  funlang = SYMBOL_LANGUAGE (msymbol);
	}
    }
  fai->called_by = CALLED_BY_PROG;
  fai->symtab = sal.symtab;
  fai->funcname = funname;
  if (!sal.symtab)
    {
      fai->funcname = funname ? funname : "??";

#if 0
      if (args)
	{
	  struct print_args_args args;

	  args.fi = fi;
	  args.func = func;
	  catch_errors (print_args_stub, (char *) &args, "", RETURN_MASK_ERROR);
	}
#endif
      if (sal.symtab && sal.symtab->filename)
	{
	  fai->filename = sal.symtab->filename;
	  fai->line = sal.line;
	}

#ifdef PC_LOAD_SEGMENT
      /* If we couldn't print out function name but if can figure out what
         load segment this pc value is from, at least print out some info
         about its load segment. */
      abort ();
      if (!funname)
	{
	  annotate_frame_where ();
	  wrap_here ("  ");
	  printf_filtered (" from %s", PC_LOAD_SEGMENT (fi->pc));
	}
#endif
    }
}

struct symbol *
my_lookup_symbol (const char *p)
{
  return lookup_symbol (p, get_current_block (), VAR_NAMESPACE, NULL, NULL);
}

void
togdb_eval_as_string_worker (const char *p)
{
  struct expression *expr;
  struct cleanup *old_chain;
  value_ptr val;
  char *p1;

  p1 = alloca (strlen (p) + 1);
  strcpy (p1, p);

  expr = parse_expression (p1);

  old_chain = make_cleanup (free_current_contents, &expr);

  val = evaluate_expression (expr);

  val_print (VALUE_TYPE (val), VALUE_CONTENTS (val), VALUE_ADDRESS (val),
	     gdb_stdout, 0, 0, 0, 0);

  do_cleanups (old_chain);
}

void 
togdb_cerror_worker_1 ()
{

}

void 
togdb_do_cleanups_ALL_CLEANUPS ()
{
  do_cleanups (ALL_CLEANUPS);
}

void 
togdb_restore_cleanups (void *p)
{
  restore_cleanups (p);
}

int 
togdb_target_has_registers ()
{
  return target_has_registers;
}

const char *
togdb_get_source_path ()
{
  extern char *source_path;

  return source_path;
}
void 
togdb_set_source_path (const char *p)
{
  extern char *source_path;

  free (source_path);
  source_path = strdup (p);

}

const char *
togdb_symtab_to_fullname (struct symtab *s)
{
  return s->fullname;
}

const char *
togdb_symtab_to_filename (struct symtab *s)
{
  return s->filename;
}

int
const 
togdb_pcreg ()
{
  return PC_REGNUM;
}
int
const 
togdb_ccrreg ()
{

#if defined(CCR_REGNUM)
  return CCR_REGNUM;
#elif defined(SR_REGNUM) && !defined(TARGET_A29K)
  /* FIXME!!  The a29k target defines SR_REGNUM to take a parameter!
   * thus, SR_REGNUM is defined, but SR_REGNUM by itself is an identifier */
  return SR_REGNUM;
#elif defined(PS_REGNUM)
  return PS_REGNUM;
#else
  ERROR
#endif

}

int reg_order[] =
{
#if defined(TARGET_H8300)
  0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12, -1
#elif defined(TARGET_SH)
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28 ,29 ,30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 41, 42, -1
#elif defined(TARGET_M68K)
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, -1
#elif defined(TARGET_SPARCLITE)
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
  64, 65, 66, 67, 68, 69, 72, 73, 74, 75, 76, 77, 78, 79, -1
#elif defined(TARGET_SPARCLET)
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
  64, 65, 66, 67, 68, 69, 72, 73, 74, 75, 76, 77, 78, 79, -1
#elif defined(TARGET_MIPS)
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
  32, 33, 34, 35, 36, 37, -1
#elif defined(TARGET_I386)
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, -1
#elif defined(TARGET_A29K)
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
  32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60,
  61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90,
  91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108,
  109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126,
  127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144,
  145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162,
  163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180,
  181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198,
  199, 200, 201, 202, 203, 204, -1
#elif defined(TARGET_V850)
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
  32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60,
  61, 62, 63, 64, -1                                                         
#else
  HEY
#endif
};

const int 
togdb_maxregs ()
{
  return NUM_REGS;
}

char *info_path;

char *
togdb_get_info_path (void)
{
  return info_path;
}

void 
togdb_set_info_path (const char *p)
{

  free (info_path);
  info_path = strdup (p);
}

void 
togdb_symtab_search_for_fullname (struct symtab *s)
{
  s->fullname = 0;
  symtab_to_filename (s);
}

int
togdb_searchpath (path, try_cwd_first, string, filename_opened)
     char *path;
     int try_cwd_first;
     const char *string;
     char **filename_opened;
{
  int res;
  char *end;
  char *s;

  s = alloca (strlen (string) + 1);
  strcpy (s, string);

  res = openp (path, try_cwd_first, s, O_RDONLY, 0, filename_opened);

  if (res >= 0)
    {
      close (res);
      return 1;
    }
  end = strrchr (s, '\\');
  if (end)
    {
      res = openp (path, try_cwd_first, end + 1, O_RDONLY, 0, filename_opened);
      if (res >= 0)
	{
	  close (res);
	  return 1;
	}
    }
  return 0;
}

/***********************************************************************/
/* extract symbol information for trivial file browser 
   - code stolen from symtab.c */
/* Probably the best thing to do would be to always generate this info
   and then pass the tree to a text or a gui back end for display */
char *operator_chars ();
struct gui_symtab_file *
gdbwin_list_symbols (const char *regexp, int class)
{
  struct gui_symtab_file *file = 0;
  struct gui_symtab_item *item = 0;

  register struct symtab *s;
  register struct partial_symtab *ps;
  register struct blockvector *bv;
  struct blockvector *prev_bv = 0;
  register struct block *b;
  register int i, j;
  register struct symbol *sym;
  struct partial_symbol *psym;
  struct objfile *objfile;
  struct minimal_symbol *msymbol;
  char *val;
  static char *classnames[]
  =
  {"variable", "function", "type", "method"};
  int found_in_file = 0;
  int found_misc = 0;
  static enum minimal_symbol_type types[]
  =
  {mst_data, mst_text, mst_abs, mst_unknown};
  static enum minimal_symbol_type types2[]
  =
  {mst_bss, mst_file_text, mst_abs, mst_unknown};
  static enum minimal_symbol_type types3[]
  =
  {mst_file_data, mst_solib_trampoline, mst_abs, mst_unknown};
  static enum minimal_symbol_type types4[]
  =
  {mst_file_bss, mst_text, mst_abs, mst_unknown};
  enum minimal_symbol_type ourtype = types[class];
  enum minimal_symbol_type ourtype2 = types2[class];
  enum minimal_symbol_type ourtype3 = types3[class];
  enum minimal_symbol_type ourtype4 = types4[class];

  if (regexp != NULL)
    {
      /* Make sure spacing is right for C++ operators.
         This is just a courtesy to make the matching less sensitive
         to how many spaces the user leaves between 'operator'
         and <TYPENAME> or <OPERATOR>. */
      char *opend;
      char *opname = operator_chars (regexp, &opend);

      if (*opname)
	{
	  int fix = -1;		/* -1 means ok; otherwise number of spaces needed. */

	  if (isalpha (*opname) || *opname == '_' || *opname == '$')
	    {
	      /* There should 1 space between 'operator' and 'TYPENAME'. */
	      if (opname[-1] != ' ' || opname[-2] == ' ')
		fix = 1;
	    }
	  else
	    {
	      /* There should 0 spaces between 'operator' and 'OPERATOR'. */
	      if (opname[-1] == ' ')
		fix = 0;
	    }
	  /* If wrong number of spaces, fix it. */
	  if (fix >= 0)
	    {
	      char *tmp = (char *) alloca (opend - opname + 10);

	      sprintf (tmp, "operator%.*s%s", fix, " ", opname);
	      regexp = tmp;
	    }
	}

      if (0 != (val = re_comp (regexp)))
	error ("Invalid regexp (%s): %s", val, regexp);
    }

  /* Search through the partial symtabs *first* for all symbols
     matching the regexp.  That way we don't have to reproduce all of
     the machinery below. */

  ALL_PSYMTABS (objfile, ps)
  {
    struct partial_symbol *bound, *gbound, *sbound;
    int keep_going = 1;

    if (ps->readin)
      continue;

    gbound = *objfile->global_psymbols.list + ps->globals_offset + ps->n_global_syms;
    sbound = *objfile->static_psymbols.list + ps->statics_offset + ps->n_static_syms;
    bound = gbound;

    /* Go through all of the symbols stored in a partial
       symtab in one loop. */
    psym = *objfile->global_psymbols.list + ps->globals_offset;
    while (keep_going)
      {
	if (psym >= bound)
	  {
	    if (bound == gbound && ps->n_static_syms != 0)
	      {
		psym = *objfile->static_psymbols.list + ps->statics_offset;
		bound = sbound;
	      }
	    else
	      keep_going = 0;
	    continue;
	  }
	else
	  {
	    QUIT;

	    /* If it would match (logic taken from loop below)
	       load the file and go on to the next one */
	    if ((regexp == NULL || SYMBOL_MATCHES_REGEXP (psym))
		&& ((class == 0 && SYMBOL_CLASS (psym) != LOC_TYPEDEF
		     && SYMBOL_CLASS (psym) != LOC_BLOCK)
		    || (class == 1 && SYMBOL_CLASS (psym) == LOC_BLOCK)
		    || (class == 2 && SYMBOL_CLASS (psym) == LOC_TYPEDEF)
		    || (class == 3 && SYMBOL_CLASS (psym) == LOC_BLOCK)))
	      {
		PSYMTAB_TO_SYMTAB (ps);
		keep_going = 0;
	      }
	  }
	psym++;
      }
  }

  /* Here, we search through the minimal symbol tables for functions
     and variables that match, and force their symbols to be read.
     This is in particular necessary for demangled variable names,
     which are no longer put into the partial symbol tables.
     The symbol will then be found during the scan of symtabs below.

     For functions, find_pc_symtab should succeed if we have debug info
     for the function, for variables we have to call lookup_symbol
     to determine if the variable has debug info.
     If the lookup fails, set found_misc so that we will rescan to print
     any matching symbols without debug info.
   */

  if (class == 0 || class == 1)
    {
      ALL_MSYMBOLS (objfile, msymbol)
      {
	if (MSYMBOL_TYPE (msymbol) == ourtype ||
	    MSYMBOL_TYPE (msymbol) == ourtype2 ||
	    MSYMBOL_TYPE (msymbol) == ourtype3 ||
	    MSYMBOL_TYPE (msymbol) == ourtype4)
	  {
	    if (regexp == NULL || SYMBOL_MATCHES_REGEXP (msymbol))
	      {
		if (0 == find_pc_symtab (SYMBOL_VALUE_ADDRESS (msymbol)))
		  {
		    if (class == 1
			|| lookup_symbol (SYMBOL_NAME (msymbol),
					  (struct block *) NULL,
					  VAR_NAMESPACE,
					0, (struct symtab **) NULL) == NULL)
		      found_misc = 1;
		  }
	      }
	  }
      }
    }

  ALL_SYMTABS (objfile, s)
  {
    found_in_file = 0;
    bv = BLOCKVECTOR (s);
    /* Often many files share a blockvector.
       Scan each blockvector only once so that
       we don't get every symbol many times.
       It happens that the first symtab in the list
       for any given blockvector is the main file.  */
    if (bv != prev_bv)
      for (i = GLOBAL_BLOCK; i <= STATIC_BLOCK; i++)
	{
	  b = BLOCKVECTOR_BLOCK (bv, i);
	  /* Skip the sort if this block is always sorted.  */
	  if (!BLOCK_SHOULD_SORT (b))
	    sort_block_syms (b);
	  for (j = 0; j < BLOCK_NSYMS (b); j++)
	    {
	      QUIT;
	      sym = BLOCK_SYM (b, j);
	      if ((regexp == NULL || SYMBOL_MATCHES_REGEXP (sym))
		  && ((class == 0 && SYMBOL_CLASS (sym) != LOC_TYPEDEF
		       && SYMBOL_CLASS (sym) != LOC_BLOCK
		       && SYMBOL_CLASS (sym) != LOC_CONST)
		      || (class == 1 && SYMBOL_CLASS (sym) == LOC_BLOCK)
		      || (class == 2 && SYMBOL_CLASS (sym) == LOC_TYPEDEF)
		      || (class == 3 && SYMBOL_CLASS (sym) == LOC_BLOCK)))
		{
		  if (!found_in_file)
		    {
		      struct gui_symtab_file *n;
		      n = (struct gui_symtab_file *) malloc (sizeof (struct gui_symtab_file));

		      n->tab = s;
		      n->opened = 0;
		      n->type = GUI_FILE;
		      n->next_file = file;
		      file = n;
		      item = 0;
		    }
		  found_in_file = 1;

		  {
		    struct gui_symtab_item *i;
		    i = (struct gui_symtab_item *) malloc (sizeof (struct gui_symtab_item));

		    i->sym = sym;
		    i->type = GUI_ITEM;
		    i->next_item = item;
		    i->parent = file;
		    item = i;
		    if (file)
		      file->items = item;
		  }
		}
	    }
	}
    prev_bv = bv;
  }
#if 0
  /* If there are no eyes, avoid all contact.  I mean, if there are
     no debug symbols, then print directly from the msymbol_vector.  */

  if (found_misc || class != 1)
    {
      found_in_file = 0;
      ALL_MSYMBOLS (objfile, msymbol)
      {
	if (MSYMBOL_TYPE (msymbol) == ourtype ||
	    MSYMBOL_TYPE (msymbol) == ourtype2 ||
	    MSYMBOL_TYPE (msymbol) == ourtype3 ||
	    MSYMBOL_TYPE (msymbol) == ourtype4)
	  {
	    if (regexp == NULL || SYMBOL_MATCHES_REGEXP (msymbol))
	      {
		/* Functions:  Look up by address. */
		if (class != 1 ||
		    (0 == find_pc_symtab (SYMBOL_VALUE_ADDRESS (msymbol))))
		  {
		    /* Variables/Absolutes:  Look up by name */
		    if (lookup_symbol (SYMBOL_NAME (msymbol),
				       (struct block *) NULL, VAR_NAMESPACE,
				       0, (struct symtab **) NULL) == NULL)
		      {
			if (!found_in_file)
			  {
			    printf_filtered ("\nNon-debugging symbols:\n");
			    found_in_file = 1;
			  }
			printf_filtered ("	%08lx  %s\n",
			     (unsigned long) SYMBOL_VALUE_ADDRESS (msymbol),
					 SYMBOL_SOURCE_NAME (msymbol));
		      }
		  }
	      }
	  }
      }
    }
#endif
  return file;
}

void 
togdb_set_breakpoint_sal (struct symtab *s, int line)
{
  struct symtab_and_line sal;

  sal.symtab = s;
  sal.line = line;
  set_breakpoint_sal (sal);
}

void 
gdbwin_list_symbols_free (struct gui_symtab_file *symt)
{
  struct gui_symtab_file *next_symt;

  for (; symt; symt = next_symt)
    {
      next_symt = symt->next_file;
      if (symt->items)
	{
	  struct gui_symtab_item *items = symt->items;
	  struct gui_symtab_item *next_item;

	  for (items = symt->items; items; items = next_item)
	    {
	      next_item = items->next_item;
	      free (items);
	    }
	}
      free (symt);
    }
}

#if 0
struct cleanup *
  my_make_cleanup PARAMS ((void (*function) (void *), void *));

#endif
