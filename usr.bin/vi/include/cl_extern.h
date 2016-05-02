/*	$OpenBSD: cl_extern.h,v 1.8 2016/05/02 18:24:25 martijn Exp $	*/

int cl_addstr(SCR *, const char *, size_t);
int cl_attr(SCR *, scr_attr_t, int);
int cl_baud(SCR *, u_long *);
int cl_bell(SCR *);
int cl_clrtoeol(SCR *);
int cl_cursor(SCR *, size_t *, size_t *);
int cl_deleteln(SCR *);
int cl_ex_adjust(SCR *, exadj_t);
int cl_insertln(SCR *);
int cl_keyval(SCR *, scr_keyval_t, unsigned char *, int *);
int cl_move(SCR *, size_t, size_t);
int cl_refresh(SCR *, int);
int cl_rename(SCR *, char *, int);
int cl_suspend(SCR *, int *);
void cl_usage(void);
int sig_init(GS *, SCR *);
int cl_event(SCR *, EVENT *, u_int32_t, int);
int cl_screen(SCR *, u_int32_t);
int cl_quit(GS *);
int cl_getcap(SCR *, char *, char **);
int cl_term_init(SCR *);
int cl_term_end(GS *);
int cl_fmap(SCR *, seq_t, char *, size_t, char *, size_t);
int cl_optchange(SCR *, int, char *, u_long *);
int cl_omesg(SCR *, CL_PRIVATE *, int);
int cl_ssize(SCR *, int, size_t *, size_t *, int *);
int cl_putchar(int);
