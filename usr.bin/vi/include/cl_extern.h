/*	$OpenBSD: cl_extern.h,v 1.6 2002/02/16 21:27:57 millert Exp $	*/

#ifndef HAVE_CURSES_ADDNSTR
int addnstr(char *, int);
#endif
#ifndef HAVE_CURSES_BEEP
void beep(void);
#endif
#ifndef HAVE_CURSES_FLASH
void flash(void);
#endif
#ifndef HAVE_CURSES_IDLOK
void idlok(WINDOW *, int);
#endif
#ifndef HAVE_CURSES_KEYPAD
int keypad(void *, int);
#endif
#ifndef HAVE_CURSES_NEWTERM
void *newterm(const char *, FILE *, FILE *);
#endif
#ifndef HAVE_CURSES_SETUPTERM
void setupterm(char *, int, int *);
#endif
#ifdef HAVE_CURSES_TIGETSTR
char *tigetstr();
#else
char *tigetstr(char *);
#endif
#ifndef HAVE_CURSES_TIGETSTR
int tigetnum(char *);
#endif
int cl_addstr(SCR *, const char *, size_t);
int cl_attr(SCR *, scr_attr_t, int);
int cl_baud(SCR *, u_long *);
int cl_bell(SCR *);
int cl_clrtoeol(SCR *);
int cl_cursor(SCR *, size_t *, size_t *);
int cl_deleteln(SCR *);
int cl_ex_adjust(SCR *, exadj_t);
int cl_insertln(SCR *);
int cl_keyval(SCR *, scr_keyval_t, CHAR_T *, int *);
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
int cl_fmap(SCR *, seq_t, CHAR_T *, size_t, CHAR_T *, size_t);
int cl_optchange(SCR *, int, char *, u_long *);
int cl_omesg(SCR *, CL_PRIVATE *, int);
int cl_ssize(SCR *, int, size_t *, size_t *, int *);
#ifdef USE_OCURSES
void cl_putchar(int);
#else
int cl_putchar(int);
#endif
