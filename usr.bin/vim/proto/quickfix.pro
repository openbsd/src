/*	$OpenBSD: quickfix.pro,v 1.1.1.1 1996/09/07 21:40:29 downsj Exp $	*/
/* quickfix.c */
int qf_init __PARMS((void));
void qf_jump __PARMS((int dir, int errornr));
void qf_list __PARMS((int all));
void qf_mark_adjust __PARMS((linenr_t line1, linenr_t line2, long amount, long amount_after));
