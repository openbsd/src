/* Generated automatically by the program `genattr'
from the machine description file `md'.  */

#ifndef PROTO
#if defined (USE_PROTOTYPES) ? USE_PROTOTYPES : defined (__STDC__)
#define PROTO(ARGS) ARGS
#else
#define PROTO(ARGS) ()
#endif
#endif
#define HAVE_ATTR_alternative
#define get_attr_alternative(insn) which_alternative
#define HAVE_ATTR_type
enum attr_type {TYPE_MOVE, TYPE_UNARY, TYPE_BINARY, TYPE_COMPARE, TYPE_LOAD, TYPE_STORE, TYPE_UNCOND_BRANCH, TYPE_BRANCH, TYPE_CALL, TYPE_CALL_NO_DELAY_SLOT, TYPE_ADDRESS, TYPE_FPLOAD, TYPE_FPSTORE, TYPE_FP, TYPE_FPCMP, TYPE_FPMUL, TYPE_FPDIV, TYPE_FPSQRT, TYPE_MULTI, TYPE_MISC};
extern enum attr_type get_attr_type ();

#define HAVE_ATTR_use_clobbered
enum attr_use_clobbered {USE_CLOBBERED_FALSE, USE_CLOBBERED_TRUE};
extern enum attr_use_clobbered get_attr_use_clobbered ();

#define HAVE_ATTR_length
extern int get_attr_length ();
extern void init_lengths ();
extern void shorten_branches PROTO((rtx));
extern int insn_default_length PROTO((rtx));
extern int insn_variable_length_p PROTO((rtx));
extern int insn_current_length PROTO((rtx));

extern int *insn_addresses;
extern int insn_current_address;

#define HAVE_ATTR_in_call_delay
enum attr_in_call_delay {IN_CALL_DELAY_FALSE, IN_CALL_DELAY_TRUE};
extern enum attr_in_call_delay get_attr_in_call_delay ();

#define DELAY_SLOTS
extern int num_delay_slots PROTO((rtx));
extern int eligible_for_delay PROTO((rtx, int, rtx, int));

extern int const_num_delay_slots PROTO((rtx));

#define HAVE_ATTR_in_branch_delay
enum attr_in_branch_delay {IN_BRANCH_DELAY_FALSE, IN_BRANCH_DELAY_TRUE};
extern enum attr_in_branch_delay get_attr_in_branch_delay ();

#define HAVE_ATTR_in_uncond_branch_delay
enum attr_in_uncond_branch_delay {IN_UNCOND_BRANCH_DELAY_FALSE, IN_UNCOND_BRANCH_DELAY_TRUE};
extern enum attr_in_uncond_branch_delay get_attr_in_uncond_branch_delay ();

#define HAVE_ATTR_in_annul_branch_delay
enum attr_in_annul_branch_delay {IN_ANNUL_BRANCH_DELAY_FALSE, IN_ANNUL_BRANCH_DELAY_TRUE};
extern enum attr_in_annul_branch_delay get_attr_in_annul_branch_delay ();

#define ANNUL_IFFALSE_SLOTS
extern int eligible_for_annul_false ();
#define INSN_SCHEDULING

extern int result_ready_cost PROTO((rtx));
extern int function_units_used PROTO((rtx));

extern struct function_unit_desc
{
  char *name;
  int bitmask;
  int multiplicity;
  int simultaneity;
  int default_cost;
  int max_issue_delay;
  int (*ready_cost_function) ();
  int (*conflict_cost_function) ();
  int max_blockage;
  unsigned int (*blockage_range_function) ();
  int (*blockage_function) ();
} function_units[];

#define FUNCTION_UNITS_SIZE 3
#define MIN_MULTIPLICITY 1
#define MAX_MULTIPLICITY 1
#define MIN_SIMULTANEITY 1
#define MAX_SIMULTANEITY 1
#define MIN_READY_COST 2
#define MAX_READY_COST 63
#define MIN_ISSUE_DELAY 1
#define MAX_ISSUE_DELAY 1
#define MIN_BLOCKAGE 2
#define MAX_BLOCKAGE 63
#define BLOCKAGE_BITS 7
#define INSN_QUEUE_SIZE 64

#define ATTR_FLAG_forward	0x1
#define ATTR_FLAG_backward	0x2
#define ATTR_FLAG_likely	0x4
#define ATTR_FLAG_very_likely	0x8
#define ATTR_FLAG_unlikely	0x10
#define ATTR_FLAG_very_unlikely	0x20
