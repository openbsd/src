#ifndef corefile_h
#define corefile_h

#include "bfd.h"

extern bfd *core_bfd;		/* bfd for core-file */
extern int core_num_syms;	/* # of entries in symbol-table */
extern asymbol **core_syms;	/* symbol table in a.out */
extern asection *core_text_sect;	/* core text section */
extern PTR core_text_space;	/* text space of a.out in core */

extern int min_insn_size;	/* size of smallest instruction, in bytes */
extern int offset_to_code;	/* offset (in bytes) of code from entry
				   address of routine */

extern void core_init PARAMS ((const char *a_out_name));
extern void core_get_text_space PARAMS ((bfd * core_bfd));
extern void core_create_function_syms PARAMS ((bfd * core_bfd));
extern void core_create_line_syms PARAMS ((bfd * core_bfd));

#endif /* corefile_h */
