/*
 * This file is tc-vax.h.
 */

#define TC_VAX 1

#define NO_RELOC 0

#define tc_aout_pre_write_hook(x)	{;}	/* not used */
#define tc_crawl_symbol_chain(a)	{;}	/* not used */
#define tc_headers_hook(a)		{;}	/* not used */
#define md_operand(x)

long md_chars_to_number PARAMS ((unsigned char *, int));

extern const struct relax_type md_relax_table[];
#define TC_GENERIC_RELAX_TABLE md_relax_table

/*
 * Local Variables:
 * comment-column: 0
 * fill-column: 131
 * End:
 */

/* end of tc-vax.h */
