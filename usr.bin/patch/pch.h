/*	$OpenBSD: pch.h,v 1.6 2003/07/22 17:18:49 otto Exp $	*/

EXT FILE	*pfp INIT(NULL);	/* patch file pointer */

void		re_patch(void);
void		open_patch_file(char *);
void		set_hunkmax(void);
bool		there_is_another_patch(void);
bool		another_hunk(void);
bool		pch_swap(void);
char		*pfetch(LINENUM);
short		pch_line_len(LINENUM);
LINENUM		pch_first(void);
LINENUM		pch_ptrn_lines(void);
LINENUM		pch_newfirst(void);
LINENUM		pch_repl_lines(void);
LINENUM		pch_end(void);
LINENUM		pch_context(void);
LINENUM		pch_hunk_beg(void);
char		pch_char(LINENUM);
char		*pfetch(LINENUM);
void		do_ed_script(void);
