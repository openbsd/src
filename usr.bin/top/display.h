/*	$OpenBSD: display.h,v 1.3 2002/02/16 21:27:55 millert Exp $	*/

/* constants needed for display.c */

/* "type" argument for new_message function */

#define  MT_standout  1
#define  MT_delayed   2

/* prototypes */
extern int display_resize(void);
extern void i_loadave(int, double *);
extern void u_loadave(int, double *);
extern void i_timeofday(time_t *);
extern void i_procstates(int, int *);
extern void u_procstates(int, int *);
extern void i_cpustates(int *);
extern void u_cpustates(int *);
extern void z_cpustates(void);
extern void i_memory(int *);
extern void u_memory(int *);
extern void i_message(void);
extern void u_message(void);
extern void i_header(char *);
extern void u_header(char *);
extern void i_process(int, char *);
extern void u_process(int, char *);
extern void u_endscreen(int);
extern void display_header(int);
extern void new_message();	/* XXX */
extern void clear_message(void);
extern int readline(char *, int, int);
extern char *printable(char *);
