/*	$OpenBSD: display.h,v 1.2 1997/08/22 07:16:27 downsj Exp $	*/

/* constants needed for display.c */

/* "type" argument for new_message function */

#define  MT_standout  1
#define  MT_delayed   2

/* prototypes */
extern int display_resize __P((void));
extern void i_loadave __P((int, double *));
extern void u_loadave __P((int, double *));
extern void i_timeofday __P((time_t *));
extern void i_procstates __P((int, int *));
extern void u_procstates __P((int, int *));
extern void i_cpustates __P((int *));
extern void u_cpustates __P((int *));
extern void z_cpustates __P((void));
extern void i_memory __P((int *));
extern void u_memory __P((int *));
extern void i_message __P((void));
extern void u_message __P((void));
extern void i_header __P((char *));
extern void u_header __P((char *));
extern void i_process __P((int, char *));
extern void u_process __P((int, char *));
extern void u_endscreen __P((int));
extern void display_header __P((int));
extern void new_message();	/* XXX */
extern void clear_message __P((void));
extern int readline __P((char *, int, int));
extern char *printable __P((char *));
