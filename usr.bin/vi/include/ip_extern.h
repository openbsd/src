/*	$OpenBSD: ip_extern.h,v 1.3 2002/02/16 21:27:58 millert Exp $	*/

int ip_addstr(SCR *, const char *, size_t);
int ip_attr(SCR *, scr_attr_t, int);
int ip_baud(SCR *, u_long *);
int ip_bell(SCR *);
void ip_busy(SCR *, const char *, busy_t);
int ip_clrtoeol(SCR *);
int ip_cursor(SCR *, size_t *, size_t *);
int ip_deleteln(SCR *);
int ip_ex_adjust(SCR *, exadj_t);
int ip_insertln(SCR *);
int ip_keyval(SCR *, scr_keyval_t, CHAR_T *, int *);
int ip_move(SCR *, size_t, size_t);
int ip_refresh(SCR *, int);
int ip_rename(SCR *);
int ip_suspend(SCR *, int *);
void ip_usage(void);
int ip_event(SCR *, EVENT *, u_int32_t, int);
int ip_screen(SCR *, u_int32_t);
int ip_quit(GS *);
int ip_term_init(SCR *);
int ip_term_end(GS *);
int ip_fmap(SCR *, seq_t, CHAR_T *, size_t, CHAR_T *, size_t);
int ip_optchange(SCR *, int, char *, u_long *);
