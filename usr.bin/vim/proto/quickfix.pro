/*	$OpenBSD: quickfix.pro,v 1.2 1996/09/21 06:23:54 downsj Exp $	*/
/* quickfix.c */
int qf_init __PARMS((void));
void qf_jump __PARMS((int dir, int errornr, int forceit));
void qf_list __PARMS((int all));
void qf_mark_adjust __PARMS((linenr_t line1, linenr_t line2, long amount, long amount_after));
