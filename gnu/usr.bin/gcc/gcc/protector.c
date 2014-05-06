/* RTL buffer overflow protection function for GNU C compiler
   Copyright (C) 1987, 88, 89, 92-7, 1998 Free Software Foundation, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

#include "config.h"
#include "system.h"
#include "machmode.h"

#include "rtl.h"
#include "tree.h"
#include "regs.h"
#include "tm_p.h"
#include "flags.h"
#include "insn-config.h"
#include "insn-flags.h"
#include "expr.h"
#include "output.h"
#include "recog.h"
#include "hard-reg-set.h"
#include "real.h"
#include "except.h"
#include "function.h"
#include "toplev.h"
#include "conditions.h"
#include "insn-attr.h"
#include "c-tree.h"
#include "optabs.h"
#include "reload.h"
#include "protector.h"


/* Warn when not issuing stack smashing protection for some reason */
int warn_stack_protector;

/* Round a value to the lowest integer less than it that is a multiple of
   the required alignment.  Avoid using division in case the value is
   negative.  Assume the alignment is a power of two.  */
#define FLOOR_ROUND(VALUE,ALIGN) ((VALUE) & ~((ALIGN) - 1))

/* Similar, but round to the next highest integer that meets the
   alignment.  */
#define CEIL_ROUND(VALUE,ALIGN)	(((VALUE) + (ALIGN) - 1) & ~((ALIGN)- 1))


/* Nonzero means use propolice as a stack protection method */
extern int flag_propolice_protection;

/* This file contains several memory arrangement functions to protect
   the return address and the frame pointer of the stack
   from a stack-smashing attack. It also
   provides the function that protects pointer variables. */

/* Nonzero if function being compiled can define string buffers that may be
   damaged by the stack-smash attack */
static int current_function_defines_vulnerable_string;
static int current_function_defines_short_string;
static int current_function_has_variable_string;
static int current_function_defines_vsized_array;
static int current_function_is_inlinable;
static int is_array;

static rtx guard_area, _guard;
static rtx function_first_insn, prologue_insert_point;

/*  */
static HOST_WIDE_INT sweep_frame_offset;
static HOST_WIDE_INT push_allocated_offset = 0;
static HOST_WIDE_INT push_frame_offset = 0;
static int saved_cse_not_expected = 0;

static int search_string_from_argsandvars PARAMS ((int caller));
static int search_string_from_local_vars PARAMS ((tree block));
static int search_pointer_def PARAMS ((tree names));
static int search_func_pointer PARAMS ((tree type));
static int check_used_flag PARAMS ((rtx x));
static void reset_used_flags_for_insns PARAMS ((rtx insn));
static void reset_used_flags_for_decls PARAMS ((tree block));
static void reset_used_flags_of_plus PARAMS ((rtx x));
static void rtl_prologue PARAMS ((rtx insn));
static void rtl_epilogue PARAMS ((rtx fnlastinsn));
static void arrange_var_order PARAMS ((tree blocks));
static void copy_args_for_protection PARAMS ((void));
static void sweep_string_variable
	PARAMS ((rtx sweep_var, HOST_WIDE_INT var_size));
static void sweep_string_in_decls
	PARAMS ((tree block, HOST_WIDE_INT sweep_offset, HOST_WIDE_INT size));
static void sweep_string_in_args
	PARAMS ((tree parms, HOST_WIDE_INT sweep_offset, HOST_WIDE_INT size));
static void sweep_string_use_of_insns
	PARAMS ((rtx insn, HOST_WIDE_INT sweep_offset, HOST_WIDE_INT size));
static void sweep_string_in_operand
	PARAMS ((rtx insn, rtx *loc,
		 HOST_WIDE_INT sweep_offset, HOST_WIDE_INT size));
static void move_arg_location
	PARAMS ((rtx insn, rtx orig, rtx new, HOST_WIDE_INT var_size));
static void change_arg_use_of_insns
	PARAMS ((rtx insn, rtx orig, rtx *new, HOST_WIDE_INT size));
static void change_arg_use_in_operand
	PARAMS ((rtx insn, rtx x, rtx orig, rtx *new, HOST_WIDE_INT size));
static void validate_insns_of_varrefs PARAMS ((rtx insn));
static void validate_operand_of_varrefs PARAMS ((rtx insn, rtx *loc));

#define SUSPICIOUS_BUF_SIZE 8

#define AUTO_BASEPTR(X) \
  (GET_CODE (X) == PLUS ? XEXP (X, 0) : X)
#define AUTO_OFFSET(X) \
  (GET_CODE (X) == PLUS ? INTVAL (XEXP (X, 1)) : 0)
#undef PARM_PASSED_IN_MEMORY
#define PARM_PASSED_IN_MEMORY(PARM) \
 (GET_CODE (DECL_INCOMING_RTL (PARM)) == MEM)
#define VIRTUAL_STACK_VARS_P(X) \
 ((X) == virtual_stack_vars_rtx || (GET_CODE (X) == REG && (X)->used))
#define TREE_VISITED(NODE) ((NODE)->common.unused_0)



void
prepare_stack_protection (inlinable)
     int inlinable;
{
  tree blocks = DECL_INITIAL (current_function_decl);
  current_function_is_inlinable = inlinable && !flag_no_inline;
  push_frame_offset = push_allocated_offset = 0;
  saved_cse_not_expected = 0;

  /*
    skip the protection if the function has no block
    or it is an inline function
  */
  if (current_function_is_inlinable) validate_insns_of_varrefs (get_insns ());
  if (! blocks || current_function_is_inlinable) return;

  current_function_defines_vulnerable_string
    = search_string_from_argsandvars (0);

  if (current_function_defines_vulnerable_string
      || flag_stack_protection)
    {
      HOST_WIDE_INT offset;
      function_first_insn = get_insns ();

      if (current_function_contains_functions) {
	  if (warn_stack_protector)
             warning ("not protecting function: it contains functions");
	  return;
      }

      /* Initialize recognition, indicating that volatile is OK.  */
      init_recog ();

      sweep_frame_offset = 0;
	
#ifdef STACK_GROWS_DOWNWARD
      /*
	frame_offset: offset to end of allocated area of stack frame.
	 It is defined in the function.c
      */

      /* the location must be before buffers */
      guard_area = assign_stack_local (BLKmode, UNITS_PER_GUARD, -1);
      PUT_MODE (guard_area, GUARD_m);
      MEM_VOLATILE_P (guard_area) = 1;

#ifndef FRAME_GROWS_DOWNWARD
      sweep_frame_offset = frame_offset;
#endif

      /* For making room for guard value, scan all insns and fix the offset
	 address of the variable that is based on frame pointer.
	 Scan all declarations of variables and fix the offset address
	 of the variable that is based on the frame pointer */
      sweep_string_variable (guard_area, UNITS_PER_GUARD);

	
      /* the location of guard area moves to the beginning of stack frame */
      if ((offset = AUTO_OFFSET(XEXP (guard_area, 0))))
	XEXP (XEXP (guard_area, 0), 1)
	  = gen_rtx_CONST_INT (VOIDmode, sweep_frame_offset);


      /* Insert prologue rtl instructions */
      rtl_prologue (function_first_insn);

      if (! current_function_has_variable_string)
	{
	  /* Generate argument saving instruction */
	  copy_args_for_protection ();

#ifndef FRAME_GROWS_DOWNWARD
	  /* If frame grows upward, character string copied from an arg
	     stays top of the guard variable.
	     So sweep the guard variable again */
	  sweep_frame_offset = CEIL_ROUND (frame_offset,
					   BIGGEST_ALIGNMENT / BITS_PER_UNIT);
	  sweep_string_variable (guard_area, UNITS_PER_GUARD);
#endif
	}
      else if (warn_stack_protector)
	warning ("not protecting variables: it has a variable length buffer");
#endif
#ifndef FRAME_GROWS_DOWNWARD
      if (STARTING_FRAME_OFFSET == 0)
	{
	  /* this may be only for alpha */
	  push_allocated_offset = BIGGEST_ALIGNMENT / BITS_PER_UNIT;
	  assign_stack_local (BLKmode, push_allocated_offset, -1);
	  sweep_frame_offset = frame_offset;
	  sweep_string_variable (const0_rtx, -push_allocated_offset);
	  sweep_frame_offset = AUTO_OFFSET (XEXP (guard_area, 0));
	}
#endif

      /* Arrange the order of local variables */
      arrange_var_order (blocks);

#ifdef STACK_GROWS_DOWNWARD
      /* Insert epilogue rtl instructions */
      rtl_epilogue (get_last_insn ());
#endif
      init_recog_no_volatile ();
    }
  else if (current_function_defines_short_string
	   && warn_stack_protector)
    warning ("not protecting function: buffer is less than %d bytes long",
	     SUSPICIOUS_BUF_SIZE);
}

/*
  search string from arguments and local variables
  caller: 0 means call from protector_stack_protection
          1 means call from push_frame
*/
static int
search_string_from_argsandvars (caller)
     int caller;
{
  tree blocks, parms;
  int string_p;

  /* saves a latest search result as a cached information */
  static tree __latest_search_decl = 0;
  static int  __latest_search_result = FALSE;

  if (__latest_search_decl == current_function_decl)
    return __latest_search_result;
  else if (caller) return FALSE;
  __latest_search_decl = current_function_decl;
  __latest_search_result = TRUE;
  
  current_function_defines_short_string = FALSE;
  current_function_has_variable_string = FALSE;
  current_function_defines_vsized_array = FALSE;

  /*
    search a string variable from local variables
  */
  blocks = DECL_INITIAL (current_function_decl);
  string_p = search_string_from_local_vars (blocks);

  if (!current_function_defines_vsized_array && current_function_calls_alloca)
    {
      current_function_has_variable_string = TRUE;
      return TRUE;
    }

  if (string_p) return TRUE;

#ifdef STACK_GROWS_DOWNWARD
  /*
    search a string variable from arguments
  */
  parms = DECL_ARGUMENTS (current_function_decl);

  for (; parms; parms = TREE_CHAIN (parms))
    if (DECL_NAME (parms) && TREE_TYPE (parms) != error_mark_node)
      {
	if (PARM_PASSED_IN_MEMORY (parms) && DECL_NAME (parms))
	  {
	    string_p = search_string_def (TREE_TYPE (parms));
	    if (string_p) return TRUE;
	  }
      }
#endif

  __latest_search_result = FALSE;
  return FALSE;
}


static int
search_string_from_local_vars (block)
     tree block;
{
  tree types;
  int found = FALSE;

  while (block && TREE_CODE(block)==BLOCK)
    {
      types = BLOCK_VARS(block);

      while (types)
	{
	  /* skip the declaration that refers an external variable */
	  /* name: types.decl.name.identifier.id                   */
	  if (! DECL_EXTERNAL (types) && ! TREE_STATIC (types)
	      && TREE_CODE (types) == VAR_DECL
	      && ! DECL_ARTIFICIAL (types)
	      && DECL_RTL_SET_P (types)
	      && GET_CODE (DECL_RTL (types)) == MEM

	      && search_string_def (TREE_TYPE (types)))
	    {
	      rtx home = DECL_RTL (types);

	      if (GET_CODE (home) == MEM
		  && (GET_CODE (XEXP (home, 0)) == MEM
		      ||
		      (GET_CODE (XEXP (home, 0)) == REG
		       && XEXP (home, 0) != virtual_stack_vars_rtx
		       && REGNO (XEXP (home, 0)) != HARD_FRAME_POINTER_REGNUM
		       && REGNO (XEXP (home, 0)) != STACK_POINTER_REGNUM
#if ARG_POINTER_REGNUM != HARD_FRAME_POINTER_REGNUM
		       && REGNO (XEXP (home, 0)) != ARG_POINTER_REGNUM
#endif
		       )))
		/* If the value is indirect by memory or by a register
		   that isn't the frame pointer then it means the object is
		   variable-sized and address through
		   that register or stack slot.
		   The protection has no way to hide pointer variables
		   behind the array, so all we can do is staying
		   the order of variables and arguments. */
		{
		  current_function_has_variable_string = TRUE;
		}
	    
	      /* found character array */
	      found = TRUE;
	    }

	  types = TREE_CHAIN(types);
	}

      if (search_string_from_local_vars (BLOCK_SUBBLOCKS (block)))
	{
	  found = TRUE;
	}

      block = BLOCK_CHAIN (block);
    }
    
  return found;
}


/*
 * search a character array from the specified type tree
 */
int
search_string_def (type)
     tree type;
{
  tree tem;
    
  if (! type)
    return FALSE;

  if (flag_strong_protection
      && TREE_CODE (type) == ARRAY_TYPE)
    return TRUE;

  switch (TREE_CODE (type))
    {
    case ARRAY_TYPE:
      /* Check if the array is a variable-sized array */
      if (TYPE_DOMAIN (type) == 0
	  || (TYPE_MAX_VALUE (TYPE_DOMAIN (type)) != 0
	      && TREE_CODE (TYPE_MAX_VALUE (TYPE_DOMAIN (type))) == NOP_EXPR))
	current_function_defines_vsized_array = TRUE;

      /* TREE_CODE( TREE_TYPE(type) ) == INTEGER_TYPE */
      if (TYPE_MAIN_VARIANT (TREE_TYPE(type)) == char_type_node
	  || TYPE_MAIN_VARIANT (TREE_TYPE(type)) == signed_char_type_node
	  || TYPE_MAIN_VARIANT (TREE_TYPE(type)) == unsigned_char_type_node)
	{
	  /* Check if the string is a variable string */
	  if (TYPE_DOMAIN (type) == 0
	      ||
	      (TYPE_MAX_VALUE (TYPE_DOMAIN (type)) != 0
	       && TREE_CODE (TYPE_MAX_VALUE (TYPE_DOMAIN (type))) == NOP_EXPR))
	    return TRUE;

	  /* Check if the string size is greater than SUSPICIOUS_BUF_SIZE */
#if SUSPICIOUS_BUF_SIZE > 0
	  if (TYPE_MAX_VALUE (TYPE_DOMAIN (type)) != 0
	      &&
	      TREE_INT_CST_LOW(TYPE_MAX_VALUE(TYPE_DOMAIN(type)))+1
	      >= SUSPICIOUS_BUF_SIZE)
	    return TRUE;

	  current_function_defines_short_string = TRUE;
#else
	  return TRUE;
#endif
	}
      
      /* to protect every functions, sweep any arrays to the frame top */
      is_array = TRUE;

      return search_string_def(TREE_TYPE (type));
	
    case UNION_TYPE:
    case QUAL_UNION_TYPE:
    case RECORD_TYPE:
      if (! TREE_VISITED (type))
	{
	  /* mark the type as having been visited already */
	  TREE_VISITED (type) = 1;

	  /* Output the name, type, position (in bits), size (in bits) of each
	     field.  */
	  for (tem = TYPE_FIELDS (type); tem; tem = TREE_CHAIN (tem))
	    {
	      /* Omit here local type decls until we know how to support
		 them. */
	      if ((TREE_CODE (tem) == TYPE_DECL)
		  || (TREE_CODE (tem) == VAR_DECL && TREE_STATIC (tem)))
	        continue;

	      if (search_string_def(TREE_TYPE (tem)))
		{
		  TREE_VISITED (type) = 0;
		  return TRUE;
		}
	    }

	  TREE_VISITED (type) = 0;
	}
      break;
	
    case POINTER_TYPE:
    case REFERENCE_TYPE:
      /* I'm not sure whether OFFSET_TYPE needs this treatment,
	 so I'll play safe and return 1.  */
    case OFFSET_TYPE:
    default:
      break;
    }

  return FALSE;
}

/*
 * examine whether the input contains frame pointer addressing
 */
int
contains_fp (op)
     rtx op;
{
  register enum rtx_code code;
  rtx x;
  int i, j;
  const char *fmt;

  x = op;
  if (x == 0)
    return FALSE;

  code = GET_CODE (x);

  switch (code)
    {
    case CONST_INT:
    case CONST_DOUBLE:
    case CONST:
    case SYMBOL_REF:
    case CODE_LABEL:
    case REG:
    case ADDRESSOF:
      return FALSE;

    case PLUS:
      if (XEXP (x, 0) == virtual_stack_vars_rtx
	  && CONSTANT_P (XEXP (x, 1)))
	return TRUE;

    default:
      break;
    }

  /* Scan all subexpressions.  */
  fmt = GET_RTX_FORMAT (code);
  for (i = 0; i < GET_RTX_LENGTH (code); i++, fmt++)
    if (*fmt == 'e')
      {
	if (contains_fp (XEXP (x, i))) return TRUE;
      }
    else if (*fmt == 'E')
      for (j = 0; j < XVECLEN (x, i); j++)
	if (contains_fp (XVECEXP (x, i, j))) return TRUE;

  return FALSE;
}


static int
search_pointer_def (type)
     tree type;
{
  tree tem;
    
  if (! type)
    return FALSE;

  switch (TREE_CODE (type))
    {
    case UNION_TYPE:
    case QUAL_UNION_TYPE:
    case RECORD_TYPE:
      if (! TREE_VISITED (type))
	{
	  /* mark the type as having been visited already */
	  TREE_VISITED (type) = 1;

	  /* Output the name, type, position (in bits), size (in bits) of each
	     field.  */
	  for (tem = TYPE_FIELDS (type); tem; tem = TREE_CHAIN (tem))
	    {
	      /* Omit here local type decls until we know how to support
		 them. */
	      if ((TREE_CODE (tem) == TYPE_DECL)
		  || (TREE_CODE (tem) == VAR_DECL && TREE_STATIC (tem)))
	        continue;

	      if (search_pointer_def(TREE_TYPE(tem)))
		{
		  TREE_VISITED (type) = 0;
		  return TRUE;
		}
	    }

	  TREE_VISITED (type) = 0;
	}
      break;

    case ARRAY_TYPE:
      return search_pointer_def (TREE_TYPE(type));
	
    case POINTER_TYPE:
    case REFERENCE_TYPE:
    case OFFSET_TYPE:
      if (TYPE_READONLY (TREE_TYPE (type)))
	{
	  /* unless this pointer contains function pointer,
	     it should be protected */
	  return search_func_pointer (TREE_TYPE (type));
	}
      return TRUE;
	
    default:
      break;
    }

  return FALSE;
}


static int
search_func_pointer (type)
     tree type;
{
  tree tem;
    
  if (! type)
    return FALSE;

  switch (TREE_CODE (type))
    {
    case UNION_TYPE:
    case QUAL_UNION_TYPE:
    case RECORD_TYPE:
	if (! TREE_VISITED (type))
	  {
	    /* mark the type as having been visited already */
	    TREE_VISITED (type) = 1;

	    /* Output the name, type, position (in bits), size (in bits) of
	       each field.  */
	    for (tem = TYPE_FIELDS (type); tem; tem = TREE_CHAIN (tem))
	      {
		if (TREE_CODE (tem) == FIELD_DECL
		    && search_func_pointer (TREE_TYPE(tem))) {
		  TREE_VISITED (type) = 0;
		  return TRUE;
		}
	      }
	    
	    TREE_VISITED (type) = 0;
	  }
	break;

    case ARRAY_TYPE:
      return search_func_pointer (TREE_TYPE(type));
	
    case POINTER_TYPE:
    case REFERENCE_TYPE:
      /* I'm not sure whether OFFSET_TYPE needs this treatment,
	 so I'll play safe and return 1.  */
    case OFFSET_TYPE:
      if (TREE_CODE (TREE_TYPE (type)) == FUNCTION_TYPE)
	return TRUE;
      return search_func_pointer (TREE_TYPE(type));
	
    default:
      break;
    }

  return FALSE;
}


/*
 * check whether the specified rtx contains PLUS rtx with used flag.
 */
static int
check_used_flag (x)
     rtx x;
{
  register int i, j;
  register enum rtx_code code;
  register const char *format_ptr;

  if (x == 0)
    return FALSE;

  code = GET_CODE (x);

  switch (code)
    {
    case REG:
    case QUEUED:
    case CONST_INT:
    case CONST_DOUBLE:
    case SYMBOL_REF:
    case CODE_LABEL:
    case PC:
    case CC0:
      return FALSE;

    case PLUS:
      if (x->used)
	return TRUE;

    default:
      break;
    }

  format_ptr = GET_RTX_FORMAT (code);
  for (i = 0; i < GET_RTX_LENGTH (code); i++)
    {
      switch (*format_ptr++)
	{
	case 'e':
	  if (check_used_flag (XEXP (x, i)))
	    return TRUE;
	  break;

	case 'E':
	  for (j = 0; j < XVECLEN (x, i); j++)
	    if (check_used_flag (XVECEXP (x, i, j)))
	      return TRUE;
	  break;
	}
    }

  return FALSE;
}


static void
reset_used_flags_for_insns (insn)
     rtx insn;
{
  register int i, j;
  register enum rtx_code code;
  register const char *format_ptr;

  for (; insn; insn = NEXT_INSN (insn))
    if (GET_CODE (insn) == INSN || GET_CODE (insn) == JUMP_INSN
	|| GET_CODE (insn) == CALL_INSN)
      {
	code = GET_CODE (insn);
	insn->used = 0;
	format_ptr = GET_RTX_FORMAT (code);

	for (i = 0; i < GET_RTX_LENGTH (code); i++)
	  {
	    switch (*format_ptr++) {
	    case 'e':
	      reset_used_flags_of_plus (XEXP (insn, i));
	      break;
			
	    case 'E':
	      for (j = 0; j < XVECLEN (insn, i); j++)
		reset_used_flags_of_plus (XVECEXP (insn, i, j));
	      break;
	    }
	  }
      }
}

static void
reset_used_flags_for_decls (block)
     tree block;
{
  tree types;
  rtx home;

  while (block && TREE_CODE(block)==BLOCK)
    {
      types = BLOCK_VARS(block);
	
      while (types)
	{
	  /* skip the declaration that refers an external variable and
	     also skip an global variable */
	  if (! DECL_EXTERNAL (types))
	    {
	      if (!DECL_RTL_SET_P (types)) goto next;
	      home = DECL_RTL (types);

	      if (GET_CODE (home) == MEM
		  && GET_CODE (XEXP (home, 0)) == PLUS
		  && GET_CODE (XEXP (XEXP (home, 0), 1)) == CONST_INT)
		{
		  XEXP (home, 0)->used = 0;
		}
	    }
	next:
	  types = TREE_CHAIN(types);
	}

      reset_used_flags_for_decls (BLOCK_SUBBLOCKS (block));

      block = BLOCK_CHAIN (block);
    }
}

/* Clear the USED bits only of type PLUS in X */

static void
reset_used_flags_of_plus (x)
     rtx x;
{
  register int i, j;
  register enum rtx_code code;
  register const char *format_ptr;

  if (x == 0)
    return;

  code = GET_CODE (x);

  /* These types may be freely shared so we needn't do any resetting
     for them.  */

  switch (code)
    {
    case REG:
    case QUEUED:
    case CONST_INT:
    case CONST_DOUBLE:
    case SYMBOL_REF:
    case CODE_LABEL:
    case PC:
    case CC0:
      return;

    case INSN:
    case JUMP_INSN:
    case CALL_INSN:
    case NOTE:
    case LABEL_REF:
    case BARRIER:
      /* The chain of insns is not being copied.  */
      return;
      
    case PLUS:
      x->used = 0;
      break;

    case CALL_PLACEHOLDER:
      reset_used_flags_for_insns (XEXP (x, 0));
      reset_used_flags_for_insns (XEXP (x, 1));
      reset_used_flags_for_insns (XEXP (x, 2));
      break;

    default:
      break;
    }

  format_ptr = GET_RTX_FORMAT (code);
  for (i = 0; i < GET_RTX_LENGTH (code); i++)
    {
      switch (*format_ptr++)
	{
	case 'e':
	  reset_used_flags_of_plus (XEXP (x, i));
	  break;

	case 'E':
	  for (j = 0; j < XVECLEN (x, i); j++)
	    reset_used_flags_of_plus (XVECEXP (x, i, j));
	  break;
	}
    }
}


static void
rtl_prologue (insn)
     rtx insn;
{
#if defined(INIT_SECTION_ASM_OP) && !defined(INVOKE__main)
#undef HAS_INIT_SECTION
#define HAS_INIT_SECTION
#endif

  rtx _val;

  for (; insn; insn = NEXT_INSN (insn))
    if (GET_CODE (insn) == NOTE
	&& NOTE_LINE_NUMBER (insn) == NOTE_INSN_FUNCTION_BEG)
      break;
  
#if !defined (HAS_INIT_SECTION)
  /* If this function is `main', skip a call to `__main'
     to run guard instruments after global initializers, etc.  */
  if (DECL_NAME (current_function_decl)
      && MAIN_NAME_P (DECL_NAME (current_function_decl))
      && DECL_CONTEXT (current_function_decl) == NULL_TREE)
    {
      rtx fbinsn = insn;
      for (; insn; insn = NEXT_INSN (insn))
	if (GET_CODE (insn) == NOTE
	    && NOTE_LINE_NUMBER (insn) == NOTE_INSN_BLOCK_BEG)
	  break;
      if (insn == 0) insn = fbinsn;
    }
#endif

  /* mark the next insn of FUNCTION_BEG insn */
  prologue_insert_point = NEXT_INSN (insn);
		
  start_sequence ();

  _guard = gen_rtx_MEM (GUARD_m, gen_rtx_SYMBOL_REF (Pmode, "__guard_local"));
  emit_move_insn ( guard_area, _guard);

  _val = get_insns ();
  end_sequence ();

  emit_insn_before (_val, prologue_insert_point);
}

static void
rtl_epilogue (insn)
     rtx insn;
{
  rtx if_false_label;
  rtx _val;
  rtx funcname;
  tree funcstr;
  int  flag_have_return = FALSE;
		
  start_sequence ();

#ifdef HAVE_return
  if (HAVE_return)
    {
      rtx insn;
      return_label = gen_label_rtx ();
      
      for (insn = prologue_insert_point; insn; insn = NEXT_INSN (insn))
	if (GET_CODE (insn) == JUMP_INSN
	    && GET_CODE (PATTERN (insn)) == RETURN
	    && GET_MODE (PATTERN (insn)) == VOIDmode)
	  {
	    rtx pat = gen_rtx_SET (VOIDmode,
				   pc_rtx,
				   gen_rtx_LABEL_REF (VOIDmode,
						      return_label));
	    PATTERN (insn) = pat;
	    flag_have_return = TRUE;
	  }


      emit_label (return_label);
    }
#endif

  /*                                          if (guard_area != _guard) */
  compare_from_rtx (guard_area, _guard, NE, 0, GUARD_m, NULL_RTX);

  if_false_label = gen_label_rtx ();		/* { */
  emit_jump_insn ( gen_beq(if_false_label));

  /* generate string for the current function name */
  funcstr = build_string (strlen(current_function_name)+1,
			  current_function_name);
  TREE_TYPE (funcstr) = build_array_type (char_type_node, 0);
  funcname = output_constant_def (funcstr, 1);

  emit_library_call (gen_rtx (SYMBOL_REF, Pmode, "__stack_smash_handler"),
		     0, VOIDmode, 2,
                     XEXP (funcname, 0), Pmode, guard_area, GUARD_m);

  /* generate RTL to return from the current function */
		
  emit_barrier ();				/* } */
  emit_label (if_false_label);

  /* generate RTL to return from the current function */
  if (DECL_RTL_SET_P (DECL_RESULT (current_function_decl)))
    use_return_register ();

#ifdef HAVE_return
  if (HAVE_return && flag_have_return)
    {
      emit_jump_insn (gen_return ());
      emit_barrier ();
    }
#endif
  
  _val = get_insns ();
  end_sequence ();

  emit_insn_after (_val, insn);
}


static void
arrange_var_order (block)
     tree block;
{
  tree types;
  HOST_WIDE_INT offset;
    
  while (block && TREE_CODE(block)==BLOCK)
    {
      /* arrange the location of character arrays in depth first.  */
      arrange_var_order (BLOCK_SUBBLOCKS (block));

      types = BLOCK_VARS (block);

      while (types)
	{
	  /* skip the declaration that refers an external variable */
	  /* name: types.decl.assembler_name.id			   */
	  if (! DECL_EXTERNAL (types) && ! TREE_STATIC (types)
	      && TREE_CODE (types) == VAR_DECL
	      && ! DECL_ARTIFICIAL (types)
	      && ! DECL_VAR_INLINE (types)	/* don't sweep inlined string */
	      && DECL_RTL_SET_P (types)
	      && GET_CODE (DECL_RTL (types)) == MEM
	      && GET_MODE (DECL_RTL (types)) == BLKmode

	      && (is_array=0, search_string_def (TREE_TYPE (types))
		  || (! current_function_defines_vulnerable_string
		      && is_array)))
	    {
	      rtx home = DECL_RTL (types);

	      if (!(GET_CODE (home) == MEM
		    && (GET_CODE (XEXP (home, 0)) == MEM
			||
			(GET_CODE (XEXP (home, 0)) == REG
			 && XEXP (home, 0) != virtual_stack_vars_rtx
			 && REGNO (XEXP (home, 0)) != HARD_FRAME_POINTER_REGNUM
			 && REGNO (XEXP (home, 0)) != STACK_POINTER_REGNUM
#if ARG_POINTER_REGNUM != HARD_FRAME_POINTER_REGNUM
			 && REGNO (XEXP (home, 0)) != ARG_POINTER_REGNUM
#endif
			 ))))
		{
		  /* found a string variable */
		  HOST_WIDE_INT var_size =
		    ((TREE_INT_CST_LOW (DECL_SIZE (types)) + BITS_PER_UNIT - 1)
		     / BITS_PER_UNIT);

		  /* confirmed it is BLKmode.  */
		  int alignment = BIGGEST_ALIGNMENT / BITS_PER_UNIT;
		  var_size = CEIL_ROUND (var_size, alignment);

		  /* skip the variable if it is top of the region
		     specified by sweep_frame_offset */
		  offset = AUTO_OFFSET (XEXP (DECL_RTL (types), 0));
		  if (offset == sweep_frame_offset - var_size)
		    sweep_frame_offset -= var_size;
		      
		  else if (offset < sweep_frame_offset - var_size)
		    sweep_string_variable (DECL_RTL (types), var_size);
		}
	    }

	  types = TREE_CHAIN(types);
	}

      block = BLOCK_CHAIN (block);
    }
}


static void
copy_args_for_protection ()
{
  tree parms = DECL_ARGUMENTS (current_function_decl);
  rtx temp_rtx;

  parms = DECL_ARGUMENTS (current_function_decl);
  for (; parms; parms = TREE_CHAIN (parms))
    if (DECL_NAME (parms) && TREE_TYPE (parms) != error_mark_node)
      {
	if (PARM_PASSED_IN_MEMORY (parms) && DECL_NAME (parms))
	  {
	    int string_p;
	    rtx seq;

	    /*
	      skip argument protection if the last argument is used
	      for the variable argument
	    */
	    /*
	      tree fntype;
	      if (TREE_CHAIN (parms) == 0)
	      {
	        fntype = TREE_TYPE (current_function_decl);

	        if ((TYPE_ARG_TYPES (fntype) != 0
		     && TREE_VALUE (tree_last (TYPE_ARG_TYPES (fntype))) 
		          != void_type_node)
	             || current_function_varargs)
	          continue;
	      }
	    */

	    string_p = search_string_def (TREE_TYPE (parms));

	    /* check if it is a candidate to move */
	    if (string_p || search_pointer_def (TREE_TYPE (parms)))
	      {
		int arg_size
		  = ((TREE_INT_CST_LOW (DECL_SIZE (parms)) + BITS_PER_UNIT - 1)
		     / BITS_PER_UNIT);
		tree passed_type = DECL_ARG_TYPE (parms);
		tree nominal_type = TREE_TYPE (parms);
		
		start_sequence ();

		if (GET_CODE (DECL_RTL (parms)) == REG)
		  {
		    rtx safe = 0;
		    
		    change_arg_use_of_insns (prologue_insert_point,
					     DECL_RTL (parms), &safe, 0);
		    if (safe)
		      {
			/* generate codes for copying the content */
			rtx movinsn = emit_move_insn (safe, DECL_RTL (parms));
		    
			/* avoid register elimination in gcse.c (COPY-PROP)*/
			PATTERN (movinsn)->volatil = 1;
			
			/* save debugger info */
			DECL_INCOMING_RTL (parms) = safe;
		      }
		  }
		else if (GET_CODE (DECL_RTL (parms)) == MEM
			 && GET_CODE (XEXP (DECL_RTL (parms), 0)) == ADDRESSOF)
		  {
		    rtx movinsn;
		    rtx safe = gen_reg_rtx (GET_MODE (DECL_RTL (parms)));

		    /* generate codes for copying the content */
		    movinsn = emit_move_insn (safe, DECL_INCOMING_RTL (parms));
		    /* avoid register elimination in gcse.c (COPY-PROP)*/
		    PATTERN (movinsn)->volatil = 1;

		    /* change the addressof information to the newly
		       allocated pseudo register */
		    emit_move_insn (DECL_RTL (parms), safe);

		    /* save debugger info */
		    DECL_INCOMING_RTL (parms) = safe;
		  }
			
		/* See if the frontend wants to pass this by invisible
		   reference.  */
		else if (passed_type != nominal_type
			 && POINTER_TYPE_P (passed_type)
			 && TREE_TYPE (passed_type) == nominal_type)
		  {
		    rtx safe = 0, orig = XEXP (DECL_RTL (parms), 0);

		    change_arg_use_of_insns (prologue_insert_point,
					     orig, &safe, 0);
		    if (safe)
		      {
			/* generate codes for copying the content */
			rtx movinsn = emit_move_insn (safe, orig);
		    
			/* avoid register elimination in gcse.c (COPY-PROP)*/
			PATTERN (movinsn)->volatil = 1;
			
			/* save debugger info */
			DECL_INCOMING_RTL (parms) = safe;
		      }
		  }

		else
		  {
		    /* declare temporary local variable DECL_NAME (parms) */
		    temp_rtx
		      = assign_stack_local (DECL_MODE (parms), arg_size,
					    DECL_MODE (parms) == BLKmode ?
					    -1 : 0);
		    
		    MEM_IN_STRUCT_P (temp_rtx)
		      = AGGREGATE_TYPE_P (TREE_TYPE (parms));
		    set_mem_alias_set (temp_rtx, get_alias_set (parms));

		    /* move_arg_location may change the contents of
		       DECL_RTL (parms). to avoid this, copies the contents */
		    SET_DECL_RTL (parms, copy_rtx (DECL_RTL (parms)));

		    /* generate codes for copying the content */
		    store_expr (parms, temp_rtx, 0);

		    /* change the reference for each instructions */
		    move_arg_location (prologue_insert_point, DECL_RTL (parms),
				       temp_rtx, arg_size);

		    /* change the location of parms variable */
		    SET_DECL_RTL (parms, temp_rtx);

		    /* change debugger info */
		    DECL_INCOMING_RTL (parms) = temp_rtx;
		  }

		seq = get_insns ();
		end_sequence ();
		emit_insn_before (seq, prologue_insert_point);

#ifdef FRAME_GROWS_DOWNWARD
		/* process the string argument */
		if (string_p && DECL_MODE (parms) == BLKmode)
		  {
		    int alignment = BIGGEST_ALIGNMENT / BITS_PER_UNIT;
		    arg_size = CEIL_ROUND (arg_size, alignment);
			
		    /* change the reference for each instructions */
		    sweep_string_variable (DECL_RTL (parms), arg_size);
		  }
#endif
	      }
	  }
      }
}


/*
  sweep a string variable to the local variable addressed
  by sweep_frame_offset, that is a last position of string variables.
*/
static void
sweep_string_variable (sweep_var, var_size)
     rtx sweep_var;
     HOST_WIDE_INT var_size;
{
  HOST_WIDE_INT sweep_offset;

  switch (GET_CODE (sweep_var))
    {
    case MEM:
      if (GET_CODE (XEXP (sweep_var, 0)) == ADDRESSOF
	  && GET_CODE (XEXP (XEXP (sweep_var, 0), 0)) == REG)
	return;
      sweep_offset = AUTO_OFFSET(XEXP (sweep_var, 0));
      break;
    case CONST_INT:
      sweep_offset = INTVAL (sweep_var);
      break;
    default:
      abort ();
    }

  /* scan all declarations of variables and fix the offset address of
     the variable based on the frame pointer */
  sweep_string_in_decls (DECL_INITIAL (current_function_decl),
			 sweep_offset, var_size);

  /* scan all argument variable and fix the offset address based on
     the frame pointer */
  sweep_string_in_args (DECL_ARGUMENTS (current_function_decl),
			sweep_offset, var_size);

  /* For making room for sweep variable, scan all insns and
     fix the offset address of the variable that is based on frame pointer */
  sweep_string_use_of_insns (function_first_insn, sweep_offset, var_size);


  /* Clear all the USED bits in operands of all insns and declarations of
     local vars */
  reset_used_flags_for_decls (DECL_INITIAL (current_function_decl));
  reset_used_flags_for_insns (function_first_insn);

  sweep_frame_offset -= var_size;
}



/*
  move an argument to the local variable addressed by frame_offset
*/
static void
move_arg_location (insn, orig, new, var_size)
     rtx  insn, orig, new;
     HOST_WIDE_INT var_size;
{
  /* For making room for sweep variable, scan all insns and
     fix the offset address of the variable that is based on frame pointer */
  change_arg_use_of_insns (insn, orig, &new, var_size);


  /* Clear all the USED bits in operands of all insns and declarations
     of local vars */
  reset_used_flags_for_insns (insn);
}


static void
sweep_string_in_decls (block, sweep_offset, sweep_size)
     tree block;
     HOST_WIDE_INT sweep_offset, sweep_size;
{
  tree types;
  HOST_WIDE_INT offset;
  rtx home;

  while (block && TREE_CODE(block)==BLOCK)
    {
      types = BLOCK_VARS(block);
	
      while (types)
	{
	  /* skip the declaration that refers an external variable and
	     also skip an global variable */
	  if (! DECL_EXTERNAL (types) && ! TREE_STATIC (types)) {
	    
	    if (!DECL_RTL_SET_P (types)) goto next;
	    home = DECL_RTL (types);

	    /* process for static local variable */
	    if (GET_CODE (home) == MEM
		&& GET_CODE (XEXP (home, 0)) == SYMBOL_REF)
	      goto next;

	    if (GET_CODE (home) == MEM
		&& XEXP (home, 0) == virtual_stack_vars_rtx)
	      {
		offset = 0;
		
		/* the operand related to the sweep variable */
		if (sweep_offset <= offset
		    && offset < sweep_offset + sweep_size)
		  {
		    offset = sweep_frame_offset - sweep_size - sweep_offset;

		    XEXP (home, 0) = plus_constant (virtual_stack_vars_rtx,
						    offset);
		    XEXP (home, 0)->used = 1;
		  }
		else if (sweep_offset <= offset
			 && offset < sweep_frame_offset)
		  {
		    /* the rest of variables under sweep_frame_offset,
		       shift the location */
		    XEXP (home, 0) = plus_constant (virtual_stack_vars_rtx,
						    -sweep_size);
		    XEXP (home, 0)->used = 1;
		  }
	      }
		
	    if (GET_CODE (home) == MEM
		&& GET_CODE (XEXP (home, 0)) == MEM)
	      {
		/* process for dynamically allocated aray */
		home = XEXP (home, 0);
	      }
		
	    if (GET_CODE (home) == MEM
		&& GET_CODE (XEXP (home, 0)) == PLUS
		&& XEXP (XEXP (home, 0), 0) == virtual_stack_vars_rtx
		&& GET_CODE (XEXP (XEXP (home, 0), 1)) == CONST_INT)
	      {
		if (! XEXP (home, 0)->used)
		  {
		    offset = AUTO_OFFSET(XEXP (home, 0));

		    /* the operand related to the sweep variable */
		    if (sweep_offset <= offset
			&& offset < sweep_offset + sweep_size)
		      {

			offset
			  += sweep_frame_offset - sweep_size - sweep_offset;
			XEXP (XEXP (home, 0), 1) = gen_rtx_CONST_INT (VOIDmode,
								      offset);

			/* mark */
			XEXP (home, 0)->used = 1;
		      }
		    else if (sweep_offset <= offset
			     && offset < sweep_frame_offset)
		      {	/* the rest of variables under sweep_frame_offset,
			   so shift the location */

			XEXP (XEXP (home, 0), 1)
			  = gen_rtx_CONST_INT (VOIDmode, offset - sweep_size);

			/* mark */
			XEXP (home, 0)->used = 1;
		      }
		  }
	      }

	  }
	next:
	  types = TREE_CHAIN(types);
	}

      sweep_string_in_decls (BLOCK_SUBBLOCKS (block),
			     sweep_offset, sweep_size);
      block = BLOCK_CHAIN (block);
    }
}


static void
sweep_string_in_args (parms, sweep_offset, sweep_size)
     tree parms;
     HOST_WIDE_INT sweep_offset, sweep_size;
{
  rtx home;
  HOST_WIDE_INT offset;
    
  for (; parms; parms = TREE_CHAIN (parms))
    if (DECL_NAME (parms) && TREE_TYPE (parms) != error_mark_node)
      {
	if (PARM_PASSED_IN_MEMORY (parms) && DECL_NAME (parms))
	  {
	    home = DECL_INCOMING_RTL (parms);

	    if (XEXP (home, 0)->used) continue;

	    offset = AUTO_OFFSET(XEXP (home, 0));

	    /* the operand related to the sweep variable */
	    if (AUTO_BASEPTR (XEXP (home, 0)) == virtual_stack_vars_rtx)
	      {
		if (sweep_offset <= offset
		    && offset < sweep_offset + sweep_size)
		  {
		    offset += sweep_frame_offset - sweep_size - sweep_offset;
		    XEXP (XEXP (home, 0), 1) = gen_rtx_CONST_INT (VOIDmode,
								  offset);

		    /* mark */
		    XEXP (home, 0)->used = 1;
		  }
		else if (sweep_offset <= offset
			 && offset < sweep_frame_offset)
		  {
		    /* the rest of variables under sweep_frame_offset,
		       shift the location */
		    XEXP (XEXP (home, 0), 1)
		      = gen_rtx_CONST_INT (VOIDmode, offset - sweep_size);

		    /* mark */
		    XEXP (home, 0)->used = 1;
		  }
	      }
	  }
      }
}


static int has_virtual_reg;

static void
sweep_string_use_of_insns (insn, sweep_offset, sweep_size)
     rtx insn;
     HOST_WIDE_INT sweep_offset, sweep_size;
{
  for (; insn; insn = NEXT_INSN (insn))
    if (GET_CODE (insn) == INSN || GET_CODE (insn) == JUMP_INSN
	|| GET_CODE (insn) == CALL_INSN)
      {
	has_virtual_reg = FALSE;
	sweep_string_in_operand (insn, &PATTERN (insn),
				 sweep_offset, sweep_size);
	sweep_string_in_operand (insn, &REG_NOTES (insn),
				 sweep_offset, sweep_size);
      }
}


static void
sweep_string_in_operand (insn, loc, sweep_offset, sweep_size)
     rtx insn, *loc;
     HOST_WIDE_INT sweep_offset, sweep_size;
{
  register rtx x = *loc;
  register enum rtx_code code;
  int i, j, k = 0;
  HOST_WIDE_INT offset;
  const char *fmt;

  if (x == 0)
    return;

  code = GET_CODE (x);

  switch (code)
    {
    case CONST_INT:
    case CONST_DOUBLE:
    case CONST:
    case SYMBOL_REF:
    case CODE_LABEL:
    case PC:
    case CC0:
    case ASM_INPUT:
    case ADDR_VEC:
    case ADDR_DIFF_VEC:
    case RETURN:
    case ADDRESSOF:
      return;
	    
    case REG:
      if (x == virtual_incoming_args_rtx
	  || x == virtual_stack_vars_rtx
	  || x == virtual_stack_dynamic_rtx
	  || x == virtual_outgoing_args_rtx
	  || x == virtual_cfa_rtx)
	has_virtual_reg = TRUE;
      return;
      
    case SET:
      /*
	skip setjmp setup insn and setjmp restore insn
	Example:
	(set (MEM (reg:SI xx)) (virtual_stack_vars_rtx)))
	(set (virtual_stack_vars_rtx) (REG))
      */
      if (GET_CODE (XEXP (x, 0)) == MEM
	  && XEXP (x, 1) == virtual_stack_vars_rtx)
	return;
      if (XEXP (x, 0) == virtual_stack_vars_rtx
	  && GET_CODE (XEXP (x, 1)) == REG)
	return;
      break;
	    
    case PLUS:
      /* Handle typical case of frame register plus constant.  */
      if (XEXP (x, 0) == virtual_stack_vars_rtx
	  && CONSTANT_P (XEXP (x, 1)))
	{
	  if (x->used) goto single_use_of_virtual_reg;
	  
	  offset = AUTO_OFFSET(x);
	  if (RTX_INTEGRATED_P (x)) k = -1; /* for inline base ptr */

	  /* the operand related to the sweep variable */
	  if (sweep_offset <= offset + k
	      && offset + k < sweep_offset + sweep_size)
	    {
	      offset += sweep_frame_offset - sweep_size - sweep_offset;

	      XEXP (x, 0) = virtual_stack_vars_rtx;
	      XEXP (x, 1) = gen_rtx_CONST_INT (VOIDmode, offset);
	      x->used = 1;
	    }
	  else if (sweep_offset <= offset + k
		   && offset + k < sweep_frame_offset)
	    {
	      /* the rest of variables under sweep_frame_offset,
		 shift the location */
	      XEXP (x, 1) = gen_rtx_CONST_INT (VOIDmode, offset - sweep_size);
	      x->used = 1;
	    }
	  
	single_use_of_virtual_reg:
	  if (has_virtual_reg) {
	    /* excerpt from insn_invalid_p in recog.c */
	    int icode = recog_memoized (insn);

	    if (icode < 0 && asm_noperands (PATTERN (insn)) < 0)
	      {
		rtx temp, seq;
		
		start_sequence ();
		temp = force_operand (x, NULL_RTX);
		seq = get_insns ();
		end_sequence ();
		
		emit_insn_before (seq, insn);
		if (! validate_change (insn, loc, temp, 0)
		    && ! validate_replace_rtx (x, temp, insn))
		  fatal_insn ("sweep_string_in_operand", insn);
	      }
	  }

	  has_virtual_reg = TRUE;
	  return;
	}

#ifdef FRAME_GROWS_DOWNWARD
      /*
	alert the case of frame register plus constant given by reg.
	*/
      else if (XEXP (x, 0) == virtual_stack_vars_rtx
	       && GET_CODE (XEXP (x, 1)) == REG)
	fatal_insn ("sweep_string_in_operand: unknown addressing", insn);
#endif

      /*
	process further subtree:
	Example:  (plus:SI (mem/s:SI (plus:SI (reg:SI 17) (const_int 8)))
	(const_int 5))
      */
      break;

    case CALL_PLACEHOLDER:
      for (i = 0; i < 3; i++)
	{
	  rtx seq = XEXP (x, i);
	  if (seq)
	    {
	      push_to_sequence (seq);
	      sweep_string_use_of_insns (XEXP (x, i),
					 sweep_offset, sweep_size);
	      XEXP (x, i) = get_insns ();
	      end_sequence ();
	    }
	}
      break;

    default:
      break;
    }

  /* Scan all subexpressions.  */
  fmt = GET_RTX_FORMAT (code);
  for (i = 0; i < GET_RTX_LENGTH (code); i++, fmt++)
    if (*fmt == 'e')
      {
	/*
	  virtual_stack_vars_rtx without offset
	  Example:
	    (set (reg:SI xx) (reg:SI 78))
	    (set (reg:SI xx) (MEM (reg:SI 78)))
	*/
	if (XEXP (x, i) == virtual_stack_vars_rtx)
	  fatal_insn ("sweep_string_in_operand: unknown fp usage", insn);
	sweep_string_in_operand (insn, &XEXP (x, i), sweep_offset, sweep_size);
      }
    else if (*fmt == 'E')
      for (j = 0; j < XVECLEN (x, i); j++)
	sweep_string_in_operand (insn, &XVECEXP (x, i, j), sweep_offset, sweep_size);
}   


/*
  change a argument variable to the local variable addressed
  by the "new" variable.
*/
static void
change_arg_use_of_insns (insn, orig, new, size)
     rtx insn, orig, *new;
     HOST_WIDE_INT size;
{
  for (; insn; insn = NEXT_INSN (insn))
    if (GET_CODE (insn) == INSN || GET_CODE (insn) == JUMP_INSN
	|| GET_CODE (insn) == CALL_INSN)
      {
	rtx seq;
	
	start_sequence ();
	change_arg_use_in_operand (insn, PATTERN (insn), orig, new, size);

	seq = get_insns ();
	end_sequence ();
	emit_insn_before (seq, insn);

	/* load_multiple insn from virtual_incoming_args_rtx have several
	   load insns. If every insn change the load address of arg
	   to frame region, those insns are moved before the PARALLEL insn
	   and remove the PARALLEL insn.  */
	if (GET_CODE (PATTERN (insn)) == PARALLEL
	    && XVECLEN (PATTERN (insn), 0) == 0)
	  delete_insn (insn);
      }
}



static void
change_arg_use_in_operand (insn, x, orig, new, size)
     rtx insn, x, orig, *new;
     HOST_WIDE_INT size;
{
  register enum rtx_code code;
  int i, j;
  HOST_WIDE_INT offset;
  const char *fmt;

  if (x == 0)
    return;

  code = GET_CODE (x);

  switch (code)
    {
    case CONST_INT:
    case CONST_DOUBLE:
    case CONST:
    case SYMBOL_REF:
    case CODE_LABEL:
    case PC:
    case CC0:
    case ASM_INPUT:
    case ADDR_VEC:
    case ADDR_DIFF_VEC:
    case RETURN:
    case REG:
    case ADDRESSOF:
      return;

    case MEM:
      /* Handle special case of MEM (incoming_args)  */
      if (GET_CODE (orig) == MEM
	  && XEXP (x, 0) == virtual_incoming_args_rtx)
	{
	  offset = 0;

	  /* the operand related to the sweep variable */
	  if (AUTO_OFFSET(XEXP (orig, 0)) <= offset &&
	      offset < AUTO_OFFSET(XEXP (orig, 0)) + size) {

	    offset = AUTO_OFFSET(XEXP (*new, 0))
	      + (offset - AUTO_OFFSET(XEXP (orig, 0)));

	    XEXP (x, 0) = plus_constant (virtual_stack_vars_rtx, offset);
	    XEXP (x, 0)->used = 1;

	    return;
	  }
	}
      break;
      
    case PLUS:
      /* Handle special case of frame register plus constant.  */
      if (GET_CODE (orig) == MEM
	  && XEXP (x, 0) == virtual_incoming_args_rtx
	  && CONSTANT_P (XEXP (x, 1))
	  && ! x->used)
	{
	  offset = AUTO_OFFSET(x);

	  /* the operand related to the sweep variable */
	  if (AUTO_OFFSET(XEXP (orig, 0)) <= offset &&
	      offset < AUTO_OFFSET(XEXP (orig, 0)) + size) {

	    offset = AUTO_OFFSET(XEXP (*new, 0))
	      + (offset - AUTO_OFFSET(XEXP (orig, 0)));

	    XEXP (x, 0) = virtual_stack_vars_rtx;
	    XEXP (x, 1) = gen_rtx_CONST_INT (VOIDmode, offset);
	    x->used = 1;

	    return;
	  }

	  /*
	    process further subtree:
	    Example:  (plus:SI (mem/s:SI (plus:SI (reg:SI 17) (const_int 8)))
	    (const_int 5))
	  */
	}
      break;

    case SET:
      /* Handle special case of "set (REG or MEM) (incoming_args)".
	 It means that the the address of the 1st argument is stored. */
      if (GET_CODE (orig) == MEM
	  && XEXP (x, 1) == virtual_incoming_args_rtx)
	{
	  offset = 0;

	  /* the operand related to the sweep variable */
	  if (AUTO_OFFSET(XEXP (orig, 0)) <= offset &&
	      offset < AUTO_OFFSET(XEXP (orig, 0)) + size) {

	    offset = AUTO_OFFSET(XEXP (*new, 0))
	      + (offset - AUTO_OFFSET(XEXP (orig, 0)));

	    XEXP (x, 1) = force_operand (plus_constant (virtual_stack_vars_rtx,
							offset), NULL_RTX);
	    XEXP (x, 1)->used = 1;

	    return;
	  }
	}
      break;

    case CALL_PLACEHOLDER:
      for (i = 0; i < 3; i++)
	{
	  rtx seq = XEXP (x, i);
	  if (seq)
	    {
	      push_to_sequence (seq);
	      change_arg_use_of_insns (XEXP (x, i), orig, new, size);
	      XEXP (x, i) = get_insns ();
	      end_sequence ();
	    }
	}
      break;

    case PARALLEL:
      for (j = 0; j < XVECLEN (x, 0); j++)
  	{
	  change_arg_use_in_operand (insn, XVECEXP (x, 0, j), orig, new, size);
	}
      if (recog_memoized (insn) < 0)
	{
	  for (i = 0, j = 0; j < XVECLEN (x, 0); j++)
	    {
	      /* if parallel insn has a insn used virtual_incoming_args_rtx,
		 the insn is removed from this PARALLEL insn.  */
	      if (check_used_flag (XVECEXP (x, 0, j)))
		{
		  emit_insn (XVECEXP (x, 0, j));
		  XVECEXP (x, 0, j) = NULL;
		}
	      else
		XVECEXP (x, 0, i++) = XVECEXP (x, 0, j);
	    }
	  PUT_NUM_ELEM (XVEC (x, 0), i);
	}
      return;

    default:
      break;
    }

  /* Scan all subexpressions.  */
  fmt = GET_RTX_FORMAT (code);
  for (i = 0; i < GET_RTX_LENGTH (code); i++, fmt++)
    if (*fmt == 'e')
      {
	if (XEXP (x, i) == orig)
	  {
	    if (*new == 0) *new = gen_reg_rtx (GET_MODE (orig));
	    XEXP (x, i) = *new;
	    continue;
	  }
	change_arg_use_in_operand (insn, XEXP (x, i), orig, new, size);
      }
    else if (*fmt == 'E')
      for (j = 0; j < XVECLEN (x, i); j++)
	{

	  if (XVECEXP (x, i, j) == orig)
	    {
	      if (*new == 0) *new = gen_reg_rtx (GET_MODE (orig));
	      XVECEXP (x, i, j) = *new;
	      continue;
	    }
	  change_arg_use_in_operand (insn, XVECEXP (x, i, j), orig, new, size);
	}
}   


static void
validate_insns_of_varrefs (insn)
     rtx insn;
{
  rtx next;

  /* Initialize recognition, indicating that volatile is OK.  */
  init_recog ();

  for (; insn; insn = next)
    {
      next = NEXT_INSN (insn);
      if (GET_CODE (insn) == INSN || GET_CODE (insn) == JUMP_INSN
	  || GET_CODE (insn) == CALL_INSN)
	{
	  /* excerpt from insn_invalid_p in recog.c */
	  int icode = recog_memoized (insn);

	  if (icode < 0 && asm_noperands (PATTERN (insn)) < 0)
	    validate_operand_of_varrefs (insn, &PATTERN (insn));
	}
    }

  init_recog_no_volatile ();
}


static void
validate_operand_of_varrefs (insn, loc)
     rtx insn, *loc;
{
  register enum rtx_code code;
  rtx x, temp, seq;
  int i, j;
  const char *fmt;

  x = *loc;
  if (x == 0)
    return;

  code = GET_CODE (x);

  switch (code)
    {
    case USE:
    case CONST_INT:
    case CONST_DOUBLE:
    case CONST:
    case SYMBOL_REF:
    case CODE_LABEL:
    case PC:
    case CC0:
    case ASM_INPUT:
    case ADDR_VEC:
    case ADDR_DIFF_VEC:
    case RETURN:
    case REG:
    case ADDRESSOF:
      return;

    case PLUS:
      /* validate insn of frame register plus constant.  */
      if (GET_CODE (x) == PLUS
	  && XEXP (x, 0) == virtual_stack_vars_rtx
	  && CONSTANT_P (XEXP (x, 1)))
	{
	  start_sequence ();

	  { /* excerpt from expand_binop in optabs.c */
	    optab binoptab = add_optab;
	    enum machine_mode mode = GET_MODE (x);
	    int icode = (int) binoptab->handlers[(int) mode].insn_code;
	    enum machine_mode mode1 = insn_data[icode].operand[2].mode;
	    rtx pat;
	    rtx xop0 = XEXP (x, 0), xop1 = XEXP (x, 1);
	    temp = gen_reg_rtx (mode);

	    /* Now, if insn's predicates don't allow offset operands,
	       put them into pseudo regs.  */

	    if (! (*insn_data[icode].operand[2].predicate) (xop1, mode1)
		&& mode1 != VOIDmode)
	      xop1 = copy_to_mode_reg (mode1, xop1);

	    pat = GEN_FCN (icode) (temp, xop0, xop1);
	    if (pat)
	      emit_insn (pat);
	    else
	      abort (); /* there must be add_optab handler.  */
	  }	      
	  seq = get_insns ();
	  end_sequence ();
	  
	  emit_insn_before (seq, insn);
	  if (! validate_change (insn, loc, temp, 0))
	    abort ();
	  return;
	}
	break;
      

    case CALL_PLACEHOLDER:
      for (i = 0; i < 3; i++)
	{
	  rtx seq = XEXP (x, i);
	  if (seq)
	    {
	      push_to_sequence (seq);
	      validate_insns_of_varrefs (XEXP (x, i));
	      XEXP (x, i) = get_insns ();
	      end_sequence ();
	    }
	}
      break;

    default:
      break;
    }

  /* Scan all subexpressions.  */
  fmt = GET_RTX_FORMAT (code);
  for (i = 0; i < GET_RTX_LENGTH (code); i++, fmt++)
    if (*fmt == 'e')
      validate_operand_of_varrefs (insn, &XEXP (x, i));
    else if (*fmt == 'E')
      for (j = 0; j < XVECLEN (x, i); j++)
	validate_operand_of_varrefs (insn, &XVECEXP (x, i, j));
}



/* Return size that is not allocated for stack frame. It will be allocated
   to modify the home of pseudo registers called from global_alloc.  */

HOST_WIDE_INT
get_frame_free_size ()
{
  if (! flag_propolice_protection)
    return 0;

  return push_allocated_offset - push_frame_offset;
}


/*
  The following codes are invoked after the instantiation of pseuso registers.

  Reorder local variables to place a peudo register after buffers to avoid
  the corruption of local variables that could be used to further corrupt
  arbitrary memory locations.
*/
#if !defined(FRAME_GROWS_DOWNWARD) && defined(STACK_GROWS_DOWNWARD)
static void push_frame
	PARAMS ((HOST_WIDE_INT var_size, HOST_WIDE_INT boundary));
static void push_frame_in_decls
	PARAMS ((tree block, HOST_WIDE_INT push_size, HOST_WIDE_INT boundary));
static void push_frame_in_args
	PARAMS ((tree parms, HOST_WIDE_INT push_size, HOST_WIDE_INT boundary));
static void push_frame_of_insns
	PARAMS ((rtx insn, HOST_WIDE_INT push_size, HOST_WIDE_INT boundary));
static void push_frame_in_operand
	PARAMS ((rtx insn, rtx orig,
		 HOST_WIDE_INT push_size, HOST_WIDE_INT boundary));
static void push_frame_of_reg_equiv_memory_loc
	PARAMS ((HOST_WIDE_INT push_size, HOST_WIDE_INT boundary));
static void push_frame_of_reg_equiv_constant
	PARAMS ((HOST_WIDE_INT push_size, HOST_WIDE_INT boundary));
static void reset_used_flags_for_push_frame PARAMS ((void));
static int check_out_of_frame_access
	PARAMS ((rtx insn, HOST_WIDE_INT boundary));
static int check_out_of_frame_access_in_operand
	PARAMS ((rtx, HOST_WIDE_INT boundary));
#endif

rtx
assign_stack_local_for_pseudo_reg (mode, size, align)
     enum machine_mode mode;
     HOST_WIDE_INT size;
     int align;
{
#if defined(FRAME_GROWS_DOWNWARD) || !defined(STACK_GROWS_DOWNWARD)
  return assign_stack_local (mode, size, align);
#else
  tree blocks = DECL_INITIAL (current_function_decl);
  rtx new;
  HOST_WIDE_INT saved_frame_offset, units_per_push, starting_frame;
  int first_call_from_purge_addressof, first_call_from_global_alloc;

  if (! flag_propolice_protection
      || size == 0
      || ! blocks
      || current_function_is_inlinable
      || ! search_string_from_argsandvars (1)
      || current_function_contains_functions)
    return assign_stack_local (mode, size, align);

  first_call_from_purge_addressof = !push_frame_offset && !cse_not_expected;
  first_call_from_global_alloc = !saved_cse_not_expected && cse_not_expected;
  saved_cse_not_expected = cse_not_expected;

  starting_frame = (STARTING_FRAME_OFFSET)?
    STARTING_FRAME_OFFSET:BIGGEST_ALIGNMENT / BITS_PER_UNIT;
  units_per_push = MAX(BIGGEST_ALIGNMENT / BITS_PER_UNIT,
		       GET_MODE_SIZE (mode));
    
  if (first_call_from_purge_addressof)
    {
      push_frame_offset = push_allocated_offset;
      if (check_out_of_frame_access (get_insns (), starting_frame))
	{
	  /* if there is an access beyond frame, push dummy region to separate
	     the address of instantiated variables */
	  push_frame (GET_MODE_SIZE (DImode), 0);
	  assign_stack_local (BLKmode, GET_MODE_SIZE (DImode), -1);
	}
    }

  if (first_call_from_global_alloc)
    {
      push_frame_offset = push_allocated_offset = 0;
      if (check_out_of_frame_access (get_insns (), starting_frame))
	{
	  if (STARTING_FRAME_OFFSET)
	    {
	      /* if there is an access beyond frame, push dummy region 
		 to separate the address of instantiated variables */
	      push_frame (GET_MODE_SIZE (DImode), 0);
	      assign_stack_local (BLKmode, GET_MODE_SIZE (DImode), -1);
	    }
	  else
	    push_allocated_offset = starting_frame;
	}
    }

  saved_frame_offset = frame_offset;
  frame_offset = push_frame_offset;

  new = assign_stack_local (mode, size, align);

  push_frame_offset = frame_offset;
  frame_offset = saved_frame_offset;
  
  if (push_frame_offset > push_allocated_offset)
    {
      push_frame (units_per_push,
		  push_allocated_offset + STARTING_FRAME_OFFSET);

      assign_stack_local (BLKmode, units_per_push, -1);
      push_allocated_offset += units_per_push;
    }

  /* At the second call from global alloc, alpha push frame and assign
     a local variable to the top of the stack */
  if (first_call_from_global_alloc && STARTING_FRAME_OFFSET == 0)
    push_frame_offset = push_allocated_offset = 0;

  return new;
#endif
}


#if !defined(FRAME_GROWS_DOWNWARD) && defined(STACK_GROWS_DOWNWARD)
/*
  push frame information for instantiating pseudo register at the top of stack.
  This is only for the "frame grows upward", it means FRAME_GROWS_DOWNWARD is 
  not defined.

  It is called by purge_addressof function and global_alloc (or reload)
  function.
*/
static void
push_frame (var_size, boundary)
     HOST_WIDE_INT var_size, boundary;
{
  reset_used_flags_for_push_frame();

  /* scan all declarations of variables and fix the offset address of
     the variable based on the frame pointer */
  push_frame_in_decls (DECL_INITIAL (current_function_decl),
		       var_size, boundary);

  /* scan all argument variable and fix the offset address based on
     the frame pointer */
  push_frame_in_args (DECL_ARGUMENTS (current_function_decl),
		      var_size, boundary);

  /* scan all operands of all insns and fix the offset address
     based on the frame pointer */
  push_frame_of_insns (get_insns (), var_size, boundary);

  /* scan all reg_equiv_memory_loc and reg_equiv_constant*/
  push_frame_of_reg_equiv_memory_loc (var_size, boundary);
  push_frame_of_reg_equiv_constant (var_size, boundary);

  reset_used_flags_for_push_frame();
}

static void
reset_used_flags_for_push_frame()
{
  int i;
  extern rtx *reg_equiv_memory_loc;
  extern rtx *reg_equiv_constant;

  /* Clear all the USED bits in operands of all insns and declarations of
     local vars */
  reset_used_flags_for_decls (DECL_INITIAL (current_function_decl));
  reset_used_flags_for_insns (get_insns ());


  /* The following codes are processed if the push_frame is called from 
     global_alloc (or reload) function */
  if (reg_equiv_memory_loc == 0) return;

  for (i=LAST_VIRTUAL_REGISTER+1; i < max_regno; i++)
    if (reg_equiv_memory_loc[i])
      {
	rtx x = reg_equiv_memory_loc[i];

	if (GET_CODE (x) == MEM
	    && GET_CODE (XEXP (x, 0)) == PLUS
	    && AUTO_BASEPTR (XEXP (x, 0)) == frame_pointer_rtx)
	  {
	    /* reset */
	    XEXP (x, 0)->used = 0;
	  }
      }

  
  if (reg_equiv_constant == 0) return;

  for (i=LAST_VIRTUAL_REGISTER+1; i < max_regno; i++)
    if (reg_equiv_constant[i])
      {
	rtx x = reg_equiv_constant[i];

	if (GET_CODE (x) == PLUS
	    && AUTO_BASEPTR (x) == frame_pointer_rtx)
	  {
	    /* reset */
	    x->used = 0;
	  }
      }
}

static void
push_frame_in_decls (block, push_size, boundary)
     tree block;
     HOST_WIDE_INT push_size, boundary;
{
  tree types;
  HOST_WIDE_INT offset;
  rtx home;

  while (block && TREE_CODE(block)==BLOCK)
    {
      types = BLOCK_VARS(block);
	
      while (types)
	{
	  /* skip the declaration that refers an external variable and
	     also skip an global variable */
	  if (! DECL_EXTERNAL (types) && ! TREE_STATIC (types))
	    {
	    
	      if (!DECL_RTL_SET_P (types)) goto next;
	      home = DECL_RTL (types);

	      /* process for static local variable */
	      if (GET_CODE (home) == MEM
		  && GET_CODE (XEXP (home, 0)) == SYMBOL_REF)
		goto next;

	      if (GET_CODE (home) == MEM
		  && GET_CODE (XEXP (home, 0)) == REG)
		{
		  if (XEXP (home, 0) != frame_pointer_rtx
		      || boundary != 0)
		    goto next;

		  XEXP (home, 0) = plus_constant (frame_pointer_rtx,
						  push_size);

		  /* mark */
		  XEXP (home, 0)->used = 1;
		}
		
	      if (GET_CODE (home) == MEM
		  && GET_CODE (XEXP (home, 0)) == MEM)
		{

		  /* process for dynamically allocated aray */
		  home = XEXP (home, 0);
		}
		
	      if (GET_CODE (home) == MEM
		  && GET_CODE (XEXP (home, 0)) == PLUS
		  && GET_CODE (XEXP (XEXP (home, 0), 1)) == CONST_INT)
		{
		  offset = AUTO_OFFSET(XEXP (home, 0));

		  if (! XEXP (home, 0)->used
		      && offset >= boundary)
		    {
		      offset += push_size;
		      XEXP (XEXP (home, 0), 1)
			= gen_rtx_CONST_INT (VOIDmode, offset);
		      
		      /* mark */
		      XEXP (home, 0)->used = 1;
		    }
		}

	    }
	next:
	  types = TREE_CHAIN(types);
	}

      push_frame_in_decls (BLOCK_SUBBLOCKS (block), push_size, boundary);
      block = BLOCK_CHAIN (block);
    }
}


static void
push_frame_in_args (parms, push_size, boundary)
     tree parms;
     HOST_WIDE_INT push_size, boundary;
{
  rtx home;
  HOST_WIDE_INT offset;
    
  for (; parms; parms = TREE_CHAIN (parms))
    if (DECL_NAME (parms) && TREE_TYPE (parms) != error_mark_node)
      {
	if (PARM_PASSED_IN_MEMORY (parms) && DECL_NAME (parms))
	  {
	    home = DECL_INCOMING_RTL (parms);
	    offset = AUTO_OFFSET(XEXP (home, 0));

	    if (XEXP (home, 0)->used || offset < boundary) continue;

	    /* the operand related to the sweep variable */
	    if (AUTO_BASEPTR (XEXP (home, 0)) == frame_pointer_rtx)
	      {
		if (XEXP (home, 0) == frame_pointer_rtx)
		  XEXP (home, 0) = plus_constant (frame_pointer_rtx,
						  push_size);
		else {
		  offset += push_size;
		  XEXP (XEXP (home, 0), 1) = gen_rtx_CONST_INT (VOIDmode,
								offset);
		}

		/* mark */
		XEXP (home, 0)->used = 1;
	      }
	  }
      }
}


static int insn_pushed;
static int *fp_equiv = 0;

static void
push_frame_of_insns (insn, push_size, boundary)
     rtx insn;
     HOST_WIDE_INT push_size, boundary;
{
  /* init fp_equiv */
  fp_equiv = (int *) xcalloc (max_reg_num (), sizeof (int));
		
  for (; insn; insn = NEXT_INSN (insn))
    if (GET_CODE (insn) == INSN || GET_CODE (insn) == JUMP_INSN
	|| GET_CODE (insn) == CALL_INSN)
      {
	rtx last;
	
	insn_pushed = FALSE;

	/* push frame in INSN operation */
	push_frame_in_operand (insn, PATTERN (insn), push_size, boundary);

	/* push frame in NOTE */
	push_frame_in_operand (insn, REG_NOTES (insn), push_size, boundary);

	/* push frame in CALL EXPR_LIST */
	if (GET_CODE (insn) == CALL_INSN)
	  push_frame_in_operand (insn, CALL_INSN_FUNCTION_USAGE (insn),
				 push_size, boundary);

	if (insn_pushed
	    && (last = try_split (PATTERN (insn), insn, 1)) != insn)
	  {
	    rtx first = NEXT_INSN (insn);
	    rtx trial = NEXT_INSN (first);
	    rtx pattern = PATTERN (trial);
	    rtx set;

	    /* update REG_EQUIV info to the first splitted insn */
	    if ((set = single_set (insn))
		&& find_reg_note (insn, REG_EQUIV, SET_SRC (set))
		&& GET_CODE (PATTERN (first)) == SET)
	      {
		REG_NOTES (first)
		  = gen_rtx_EXPR_LIST (REG_EQUIV,
				       SET_SRC (PATTERN (first)),
				       REG_NOTES (first));
	      }

	    /* copy the first insn of splitted insns to the original insn and
	       delete the first insn,
	       because the original insn is pointed from records:
	       insn_chain, reg_equiv_init, used for global_alloc.  */
	    if (cse_not_expected)
	      {
		add_insn_before (insn, first);
		
		/* Copy the various flags, and other information.  */
		memcpy (insn, first, sizeof (struct rtx_def) - sizeof (rtunion));
		PATTERN (insn) = PATTERN (first);
		INSN_CODE (insn) = INSN_CODE (first);
		LOG_LINKS (insn) = LOG_LINKS (first);
		REG_NOTES (insn) = REG_NOTES (first);

		/* then remove the first insn of splitted insns.  */
		remove_insn (first);
		INSN_DELETED_P (first) = 1;
	      }

	    if (GET_CODE (pattern) == SET
		&& GET_CODE (XEXP (pattern, 0)) == REG
		&& GET_CODE (XEXP (pattern, 1)) == PLUS
		&& XEXP (pattern, 0) == XEXP (XEXP (pattern, 1), 0)
		&& CONSTANT_P (XEXP (XEXP (pattern, 1), 1)))
	      {
		rtx offset = XEXP (XEXP (pattern, 1), 1);
		fp_equiv[REGNO (XEXP (pattern, 0))] = INTVAL (offset);

		delete_insn (trial);
	      }

	    insn = last;
	  }
      }

  /* Clean up.  */
  free (fp_equiv);
}


static void
push_frame_in_operand (insn, orig, push_size, boundary)
     rtx insn, orig;
     HOST_WIDE_INT push_size, boundary;
{
  register rtx x = orig, prev_insn;
  register enum rtx_code code;
  int i, j;
  HOST_WIDE_INT offset;
  const char *fmt;

  if (x == 0)
    return;

  code = GET_CODE (x);

  switch (code)
    {
    case CONST_INT:
    case CONST_DOUBLE:
    case CONST:
    case SYMBOL_REF:
    case CODE_LABEL:
    case PC:
    case CC0:
    case ASM_INPUT:
    case ADDR_VEC:
    case ADDR_DIFF_VEC:
    case RETURN:
    case REG:
    case ADDRESSOF:
    case USE:
      return;
	    
    case SET:
      /*
	skip setjmp setup insn and setjmp restore insn
	alpha case:
	(set (MEM (reg:SI xx)) (frame_pointer_rtx)))
	(set (frame_pointer_rtx) (REG))
      */
      if (GET_CODE (XEXP (x, 0)) == MEM
	  && XEXP (x, 1) == frame_pointer_rtx)
	return;
      if (XEXP (x, 0) == frame_pointer_rtx
	  && GET_CODE (XEXP (x, 1)) == REG)
	return;

      /*
	powerpc case: restores setjmp address
	(set (frame_pointer_rtx) (plus frame_pointer_rtx const_int -n))
	or
	(set (reg) (plus frame_pointer_rtx const_int -n))
	(set (frame_pointer_rtx) (reg))
      */
      if (GET_CODE (XEXP (x, 0)) == REG
	  && GET_CODE (XEXP (x, 1)) == PLUS
	  && XEXP (XEXP (x, 1), 0) == frame_pointer_rtx
	  && CONSTANT_P (XEXP (XEXP (x, 1), 1))
	  && INTVAL (XEXP (XEXP (x, 1), 1)) < 0)
	{
	  x = XEXP (x, 1);
	  offset = AUTO_OFFSET(x);
	  if (x->used || -offset < boundary)
	    return;

	  XEXP (x, 1) = gen_rtx_CONST_INT (VOIDmode, offset - push_size);
	  x->used = 1; insn_pushed = TRUE;
	  return;
	}

      /* reset fp_equiv register */
      else if (GET_CODE (XEXP (x, 0)) == REG
	  && fp_equiv[REGNO (XEXP (x, 0))])
	fp_equiv[REGNO (XEXP (x, 0))] = 0;

      /* propagate fp_equiv register */
      else if (GET_CODE (XEXP (x, 0)) == REG
	       && GET_CODE (XEXP (x, 1)) == REG
	       && fp_equiv[REGNO (XEXP (x, 1))])
	if (REGNO (XEXP (x, 0)) <= LAST_VIRTUAL_REGISTER
	    || (reg_renumber != 0 && reg_renumber[REGNO (XEXP (x, 0))] >= 0))
	  fp_equiv[REGNO (XEXP (x, 0))] = fp_equiv[REGNO (XEXP (x, 1))];
      break;

    case MEM:
      if (XEXP (x, 0) == frame_pointer_rtx
	  && boundary == 0)
	{
	  XEXP (x, 0) = plus_constant (frame_pointer_rtx, push_size);
	  XEXP (x, 0)->used = 1; insn_pushed = TRUE;
	  return;
	}
      break;
      
    case PLUS:
      offset = AUTO_OFFSET(x);
      prev_insn = prev_nonnote_insn (insn);

      /* Handle special case of frame register plus constant.  */
      if (CONSTANT_P (XEXP (x, 1))
	  && XEXP (x, 0) == frame_pointer_rtx)
	{
	  if (x->used || offset < boundary)
	    return;

	  XEXP (x, 1) = gen_rtx_CONST_INT (VOIDmode, offset + push_size);
	  x->used = 1; insn_pushed = TRUE;

	  return;
	}
      /*
	Handle alpha case:
	 (plus:SI (subreg:SI (reg:DI 63 FP) 0) (const_int 64 [0x40]))
      */
      if (CONSTANT_P (XEXP (x, 1))
	  && GET_CODE (XEXP (x, 0)) == SUBREG
	  && SUBREG_REG (XEXP (x, 0)) == frame_pointer_rtx)
	{
	  if (x->used || offset < boundary)
	    return;

	  XEXP (x, 1) = gen_rtx_CONST_INT (VOIDmode, offset + push_size);
	  x->used = 1; insn_pushed = TRUE;

	  return;
	}
      /*
	Handle powerpc case:
	 (set (reg x) (plus fp const))
	 (set (.....) (... (plus (reg x) (const B))))
      */
      else if (CONSTANT_P (XEXP (x, 1))
	       && GET_CODE (XEXP (x, 0)) == REG
	       && fp_equiv[REGNO (XEXP (x, 0))])
	{
	  if (x->used) return;

	  offset += fp_equiv[REGNO (XEXP (x, 0))];

	  XEXP (x, 1) = gen_rtx_CONST_INT (VOIDmode, offset);
	  x->used = 1; insn_pushed = TRUE;

	  return;
	}
      /*
	Handle special case of frame register plus reg (constant).
	 (set (reg x) (const B))
	 (set (....) (...(plus fp (reg x))))
      */
      else if (XEXP (x, 0) == frame_pointer_rtx
	       && GET_CODE (XEXP (x, 1)) == REG
	       && prev_insn
	       && PATTERN (prev_insn)
	       && SET_DEST (PATTERN (prev_insn)) == XEXP (x, 1)
	       && CONSTANT_P (SET_SRC (PATTERN (prev_insn))))
	{
	  HOST_WIDE_INT offset = INTVAL (SET_SRC (PATTERN (prev_insn)));

	  if (x->used || offset < boundary)
	    return;
	  
	  SET_SRC (PATTERN (prev_insn))
	    = gen_rtx_CONST_INT (VOIDmode, offset + push_size);
	  x->used = 1;
	  XEXP (x, 1)->used = 1;

	  return;
	}
      /* Handle special case of frame register plus reg (used).  */
      else if (XEXP (x, 0) == frame_pointer_rtx
	       && XEXP (x, 1)->used)
	{
	  x->used = 1;
	  return;
	}
      /*
	process further subtree:
	Example:  (plus:SI (mem/s:SI (plus:SI (reg:SI 17) (const_int 8)))
	(const_int 5))
      */
      break;

    case CALL_PLACEHOLDER:
      push_frame_of_insns (XEXP (x, 0), push_size, boundary);
      push_frame_of_insns (XEXP (x, 1), push_size, boundary);
      push_frame_of_insns (XEXP (x, 2), push_size, boundary);
      break;

    default:
      break;
    }

  /* Scan all subexpressions.  */
  fmt = GET_RTX_FORMAT (code);
  for (i = 0; i < GET_RTX_LENGTH (code); i++, fmt++)
    if (*fmt == 'e')
      {
	if (XEXP (x, i) == frame_pointer_rtx && boundary == 0)
	  fatal_insn ("push_frame_in_operand", insn);
	push_frame_in_operand (insn, XEXP (x, i), push_size, boundary);
      }
    else if (*fmt == 'E')
      for (j = 0; j < XVECLEN (x, i); j++)
	push_frame_in_operand (insn, XVECEXP (x, i, j), push_size, boundary);
}   

static void
push_frame_of_reg_equiv_memory_loc (push_size, boundary)
     HOST_WIDE_INT push_size, boundary;
{
  int i;
  extern rtx *reg_equiv_memory_loc;

  /* This function is processed if the push_frame is called from 
     global_alloc (or reload) function */
  if (reg_equiv_memory_loc == 0) return;

  for (i=LAST_VIRTUAL_REGISTER+1; i < max_regno; i++)
    if (reg_equiv_memory_loc[i])
      {
	rtx x = reg_equiv_memory_loc[i];
	int offset;

	if (GET_CODE (x) == MEM
	    && GET_CODE (XEXP (x, 0)) == PLUS
	    && XEXP (XEXP (x, 0), 0) == frame_pointer_rtx)
	  {
	    offset = AUTO_OFFSET(XEXP (x, 0));
	    
	    if (! XEXP (x, 0)->used
		&& offset >= boundary)
	      {
		offset += push_size;
		XEXP (XEXP (x, 0), 1) = gen_rtx_CONST_INT (VOIDmode, offset);

		/* mark */
		XEXP (x, 0)->used = 1;
	      }
	  }
	else if (GET_CODE (x) == MEM
		 && XEXP (x, 0) == frame_pointer_rtx
		 && boundary == 0)
	  {
	    XEXP (x, 0) = plus_constant (frame_pointer_rtx, push_size);
	    XEXP (x, 0)->used = 1; insn_pushed = TRUE;
	  }
      }
}

static void
push_frame_of_reg_equiv_constant (push_size, boundary)
     HOST_WIDE_INT push_size, boundary;
{
  int i;
  extern rtx *reg_equiv_constant;

  /* This function is processed if the push_frame is called from 
     global_alloc (or reload) function */
  if (reg_equiv_constant == 0) return;

  for (i=LAST_VIRTUAL_REGISTER+1; i < max_regno; i++)
    if (reg_equiv_constant[i])
      {
	rtx x = reg_equiv_constant[i];
	int offset;

	if (GET_CODE (x) == PLUS
	    && XEXP (x, 0) == frame_pointer_rtx)
	  {
	    offset = AUTO_OFFSET(x);
	    
	    if (! x->used
		&& offset >= boundary)
	      {
		offset += push_size;
		XEXP (x, 1) = gen_rtx_CONST_INT (VOIDmode, offset);

		/* mark */
		x->used = 1;
	      }
	  }
	else if (x == frame_pointer_rtx
		 && boundary == 0)
	  {
	    reg_equiv_constant[i]
	      = plus_constant (frame_pointer_rtx, push_size);
	    reg_equiv_constant[i]->used = 1; insn_pushed = TRUE;
	  }
      }
}

static int
check_out_of_frame_access (insn, boundary)
     rtx insn;
     HOST_WIDE_INT boundary;
{
  for (; insn; insn = NEXT_INSN (insn))
    if (GET_CODE (insn) == INSN || GET_CODE (insn) == JUMP_INSN
	|| GET_CODE (insn) == CALL_INSN)
      {
	if (check_out_of_frame_access_in_operand (PATTERN (insn), boundary))
	  return TRUE;
      }
  return FALSE;
}


static int
check_out_of_frame_access_in_operand (orig, boundary)
     rtx orig;
     HOST_WIDE_INT boundary;
{
  register rtx x = orig;
  register enum rtx_code code;
  int i, j;
  const char *fmt;

  if (x == 0)
    return FALSE;

  code = GET_CODE (x);

  switch (code)
    {
    case CONST_INT:
    case CONST_DOUBLE:
    case CONST:
    case SYMBOL_REF:
    case CODE_LABEL:
    case PC:
    case CC0:
    case ASM_INPUT:
    case ADDR_VEC:
    case ADDR_DIFF_VEC:
    case RETURN:
    case REG:
    case ADDRESSOF:
      return FALSE;
	    
    case MEM:
      if (XEXP (x, 0) == frame_pointer_rtx)
	if (0 < boundary) return TRUE;
      break;
      
    case PLUS:
      /* Handle special case of frame register plus constant.  */
      if (CONSTANT_P (XEXP (x, 1))
	  && XEXP (x, 0) == frame_pointer_rtx)
	{
	  if (0 <= AUTO_OFFSET(x)
	      && AUTO_OFFSET(x) < boundary) return TRUE;
	  return FALSE;
	}
      /*
	process further subtree:
	Example:  (plus:SI (mem/s:SI (plus:SI (reg:SI 17) (const_int 8)))
	(const_int 5))
      */
      break;

    case CALL_PLACEHOLDER:
      if (check_out_of_frame_access (XEXP (x, 0), boundary)) return TRUE;
      if (check_out_of_frame_access (XEXP (x, 1), boundary)) return TRUE;
      if (check_out_of_frame_access (XEXP (x, 2), boundary)) return TRUE;
      break;

    default:
      break;
    }

  /* Scan all subexpressions.  */
  fmt = GET_RTX_FORMAT (code);
  for (i = 0; i < GET_RTX_LENGTH (code); i++, fmt++)
    if (*fmt == 'e')
      {
	if (check_out_of_frame_access_in_operand (XEXP (x, i), boundary))
	  return TRUE;
      }
    else if (*fmt == 'E')
      for (j = 0; j < XVECLEN (x, i); j++)
	if (check_out_of_frame_access_in_operand (XVECEXP (x, i, j), boundary))
	  return TRUE;

  return FALSE;
}
#endif
