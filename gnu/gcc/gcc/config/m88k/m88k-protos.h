/* Definitions of target machine for GNU compiler for
   Motorola m88100 in an 88open OCS/BCS environment.
   Copyright (C) 2000 Free Software Foundation, Inc.
   Contributed by Michael Tiemann (tiemann@cygnus.com).
   Currently maintained by (gcc@dg-rtp.dg.com)

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GCC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to
the Free Software Foundation, 51 Franklin Street, Fifth Floor,
Boston, MA 02110-1301, USA.  */

#ifdef RTX_CODE
extern int m88k_debugger_offset (rtx, int);
extern void emit_bcnd (enum rtx_code, rtx);
extern void expand_block_move (rtx, rtx, rtx *);
extern void print_operand (FILE *, rtx, int);
extern void print_operand_address (FILE *, rtx);
extern const char *output_load_const_int (enum machine_mode, rtx *);
extern const char *output_load_const_float (rtx *);
extern const char *output_load_const_double (rtx *);
extern const char *output_load_const_dimode (rtx *);
extern const char *output_and (rtx[]);
extern const char *output_ior (rtx[]);
extern const char *output_xor (rtx[]);
extern const char *output_call (rtx[], rtx);

extern struct rtx_def *emit_test (enum rtx_code, enum machine_mode);
extern struct rtx_def *legitimize_address (int, rtx, rtx, rtx);
extern struct rtx_def *legitimize_operand (rtx, enum machine_mode);

extern bool pic_address_needs_scratch (rtx);
extern bool symbolic_address_p (rtx);
extern int condition_value (rtx);
extern int emit_move_sequence (rtx *, enum machine_mode, rtx);
extern bool mostly_false_jump (rtx, rtx);
extern bool real_power_of_2_operand (rtx);
#ifdef TREE_CODE
extern void m88k_va_start (tree, rtx);
#endif /* TREE_CODE */
#endif /* RTX_CODE */

extern bool null_prologue (void);
extern bool integer_ok_for_set (unsigned int);
extern void m88k_layout_frame (void);
extern void m88k_expand_prologue (void);
extern void m88k_expand_epilogue (void);
extern void output_function_profiler (FILE *, int, const char *);
extern enum m88k_instruction classify_integer (enum machine_mode, int);
extern bool mak_mask_p (int);

#ifdef TREE_CODE
extern struct rtx_def *m88k_function_arg (CUMULATIVE_ARGS, enum machine_mode,
					  tree, int);
extern void m88k_function_arg_advance (CUMULATIVE_ARGS *, enum machine_mode,
				       tree, int);
#endif /* TREE_CODE */

extern void m88k_override_options (void);
